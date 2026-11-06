import ast
import json
from pathlib import Path
import tempfile
from unittest.mock import patch

import torch
import torch.distributed as dist
import torch.multiprocessing as mp

from tools import e4_stage12_chain as chain
from train.e4_expert_window import mode_window_seconds
from train.e4_stage12 import new_cursor, local_minibatch


def test_snapshot_counts_allocated_gpus_and_waiting_requests():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        state = dict(next_resource_check_at=0, resource_triggers=[], jobs={}, request=dict(world=8))
        jobs = 'JobId=42 JobState=RUNNING UserId=other(2) NodeList=node TRES=gres/gpu=5\n'
        node = 'CfgTRES=cpu=56,gres/gpu=8 AllocTRES=cpu=5,gres/gpu=5'
        with patch.dict(chain.os.environ, USER='me', STAGE12_NODE='node', STAGE12_PARTITION='p'), \
             patch.object(chain, 'command', side_effect=[jobs, node]):
            result, queried = chain.snapshot(root, state, 'one')
            assert queried and result['world'] == 3
            assert chain.snapshot(root, state, 'one')[1] is False
        state['next_resource_check_at'] = 0
        jobs += ('JobId=43 JobState=PENDING UserId=other(2) Partition=p '
                 'Reason=Resources TRES=gres/gpu=1\n')
        with patch.dict(chain.os.environ, USER='me', STAGE12_NODE='node', STAGE12_PARTITION='p'), \
             patch.object(chain, 'command', side_effect=[jobs, node]):
            result, _ = chain.snapshot(root, state, 'two')
        assert result['world'] == 2


def test_running_three_gpus_keeps_training_while_other_jobs_are_pending():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root/'training').mkdir()
        jobs = {'10': dict(world=3, wall_seconds=1000, settled=False, state='RUNNING')}
        state = dict(jobs=jobs, training_root=directory)
        (root/'state.json').write_text(json.dumps(state))
        resources = dict(world=3, pending=[dict(JobId='42')],
                         all_jobs_original='JobId=10 JobState=RUNNING Reason=None Priority=1')
        with patch.object(chain, 'settle'), \
             patch.object(chain, 'snapshot', return_value=(resources, True)), \
             patch.object(chain, 'command') as command:
            chain.watch(root, 'test')
        assert not (root/'training/stop-requested').exists()
        command.assert_not_called()


def test_admin_held_job_is_never_resized_or_released():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        state = dict(jobs={'4557': dict(world=8, settled=False)})
        (root/'state.json').write_text(json.dumps(state))
        resources = dict(world=3, all_jobs_original=(
            'JobId=4557 JobState=PENDING Reason=JobHeldAdmin Priority=0'))
        with patch.object(chain, 'settle'), \
             patch.object(chain, 'snapshot', return_value=(resources, True)), \
             patch.object(chain, 'command') as command:
            chain.watch(root, 'test')
        command.assert_not_called()


def test_resize_budget_uses_new_gpu_count_and_keeps_historical_upper_bounds():
    state = dict(limit_node_seconds=10000, charged_node_seconds=2000,
        request=dict(world=8), jobs={
            'held': dict(world=8, wall_seconds=1000, settled=False),
            'pending': dict(world=8, wall_seconds=7200, settled=False)})
    assert chain.available_wall(state, 7, 'pending') == 7980
    assert state['jobs']['held']['wall_seconds'] == 1000


def test_supplemental_timing_sums_serial_shards_per_physical_gpu():
    records = [dict(windows={'one': dict(seconds=owner+1, final=True,
                                       failed=False, worker_rank=owner % 3)}) for owner in range(8)]
    assert mode_window_seconds(records) == 15
    legacy = [dict(windows={'old': dict(seconds=i+1)}) for i in range(8)]
    assert mode_window_seconds(legacy) == 8
    for world in range(1, 9):
        ownership = [owner for rank in range(world) for owner in range(rank, 8, world)]
        assert sorted(ownership) == list(range(8))


def test_serial_combat_shards_share_the_remaining_one_hour_budget():
    tree = ast.parse(Path('train/e4_expert_window.py').read_text())
    assignment = next(node for node in ast.walk(tree) if isinstance(node, ast.Assign)
                      and any(ast.unparse(target) == 'search.deadline' for target in node.targets))
    value = compile(ast.Expression(assignment.value), '<combat-deadline>', 'eval')
    for mode in ('argmax','combat','card'):
        clock = dict(mode=mode, deadline=10000., mode_started=100., old=dict(seconds=3500.))
        assert eval(value, dict(clock, started=100.)) == 200.
        assert eval(value, dict(clock, started=180.)) == 200.


def test_world_change_drops_old_global_cursor_and_expert_round_once():
    source = Path('tools/e4_stage12.py').read_text()
    tree = ast.parse(source)
    block = next(node for node in ast.walk(tree) if isinstance(node, ast.If)
                 and ast.unparse(node.test) == "state['world'] != world")
    expert = dict(round={'records': ['already-applied']}, preview_rng=[42], capacity=31)
    namespace = dict(state={'world': 8}, world=3,
        cursor=dict(optimizer_steps=17), round_state=dict(ready='old', seconds=12.),
        expert_saved=expert, json=json)
    with patch('builtins.print') as log:
        exec(compile(ast.Module(body=[block], type_ignores=[]), '<world-resume>', 'exec'), namespace)
    assert namespace['cursor'] is None and namespace['round_state']['ready'] is None
    assert namespace['round_state']['seconds'] == 12.
    assert expert == dict(round=None, preview_rng=[42], capacity=31)
    assert json.loads(log.call_args.args[0])['retained_partial_optimizer_steps'] == 17


def cursor_worker(rank, rendezvous):
    dist.init_process_group('gloo', init_method='file://' + rendezvous, rank=rank, world_size=3)
    count = 3-rank
    fragment = dict(actions=torch.zeros(count), action_valid=torch.ones(count, dtype=torch.bool),
                    advantages=torch.arange(count, dtype=torch.float32),
                    data=torch.tensor([1, 0, 1, 0], dtype=torch.uint8).repeat(count),
                    offsets=torch.arange(count+1, dtype=torch.int64)*4)
    cursor = new_cursor(fragment, torch.Generator().manual_seed(42), 3)
    assert cursor['counts'] == [3, 2, 1]
    selected, total = local_minibatch(cursor, rank)
    assert total == 6 and sorted(selected.tolist()) == list(range(count))
    model = torch.nn.parallel.DistributedDataParallel(torch.nn.Linear(1, 1, bias=False))
    with torch.no_grad():
        model.module.weight.fill_(1.)
    optimizer = torch.optim.Adam(model.parameters(), lr=.01)
    loss = model(torch.ones(count, 1)).sum() * 3 / total
    loss.backward()
    assert torch.allclose(model.module.weight.grad, torch.ones(1, 1))
    optimizer.step()
    assert torch.allclose(model.module.weight, torch.tensor([[.99]]))
    assert optimizer.state[model.module.weight]['step'] == 1
    dist.destroy_process_group()


def test_three_rank_uneven_collection_has_one_global_mean_gradient():
    with tempfile.TemporaryDirectory() as directory:
        mp.spawn(cursor_worker, args=(str(Path(directory)/'rendezvous'),), nprocs=3, join=True)
