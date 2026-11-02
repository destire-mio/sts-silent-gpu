"""Focused CPU regressions for #138 expert isolation and continuation."""
from contextlib import nullcontext
from copy import deepcopy
import math
from types import SimpleNamespace
import torch
import pytest
from train import e4_expert_iteration as expert
from train import e4_stage12 as learner
from train.e4_expert_window import due_combat_comparison, complete_seed_set


def test_global_ce_mean_uneven_ranks_and_minibatches():
    torch.manual_seed(9)
    features = torch.randn(11, 3, dtype=torch.double)
    targets = torch.arange(11) % 3
    legal = torch.ones(11, 3, dtype=torch.bool)
    legal[::2, 2] = targets[::2] == 2
    weights = torch.randn(3, 3, dtype=torch.double, requires_grad=True)
    expected, _ = expert.ce_scalar(features @ weights, legal, targets, 1, 11, 1)
    reference = torch.autograd.grad(expected, weights)[0]
    result = torch.zeros_like(weights)
    # DDP averages rank gradients; averaging the K Adam minibatch objectives
    # must recover beta * mean over every actual expert sample, with no padding.
    for ids in (torch.arange(0,2), torch.arange(2,11), torch.arange(0)):
        for k in range(4):
            subset = ids[k::4]
            if len(subset):
                scalar, _ = expert.ce_scalar(features[subset] @ weights, legal[subset],
                                             targets[subset], 3, 11, 4)
                result += torch.autograd.grad(scalar, weights)[0] / (3*4)
    assert torch.allclose(result, reference, atol=1e-7)


class TinyPolicy(torch.nn.Module):
    def __init__(self):
        super().__init__()
        self.actor = torch.nn.Linear(3, 3)
        self.value = torch.nn.Linear(3, 1)
    def forward(self, inputs):
        return self.actor(inputs['features']), self.value(inputs['features']).squeeze(-1)
    def no_sync(self):
        return nullcontext()


def test_adam_step_combines_ce_without_expert_ppo_or_value(monkeypatch):
    monkeypatch.setattr(learner.dist, 'all_reduce', lambda *a, **k: None)
    def packed(data, offsets, ids, device):
        return dict(features=data[ids], candidate_mask=torch.ones(len(ids),3,dtype=torch.bool))
    monkeypatch.setattr(learner, 'packed_inputs', packed)
    monkeypatch.setattr(expert, 'packed_inputs', packed)
    torch.manual_seed(15)
    model = TinyPolicy()
    reference = deepcopy(model)
    normal = torch.randn(7,3)
    labels = torch.randn(5,3)
    actions = torch.arange(7) % 3
    target = torch.arange(5) % 3
    old = reference(dict(features=normal))[0].log_softmax(-1).detach()[torch.arange(7),actions]
    returns = torch.randn(7)
    advantages = torch.randn(7)
    f = dict(data=normal,offsets=torch.arange(8),actions=actions, logs=old,
             advantages=advantages,returns=returns,policy_mask=torch.ones(7,3,dtype=torch.bool))
    cursor = dict(fragment=f,indices=torch.arange(7),counts=[7],order=torch.arange(7),
        start=0,epoch=0,finite=torch.tensor(True),totals=torch.zeros(8,dtype=torch.double),
        optimizer_steps=0,optimizer_attempts=0,amp_skipped_steps=0,
        expert=dict(data=labels,offsets=torch.arange(6),actions=target,
                    policy_mask=torch.ones(5,3,dtype=torch.bool)),
        expert_total=5,expert_minibatches=1,expert_ce_stats=[0.,0.],expert_ce_seconds=0.)
    # local_minibatch sorts packed native record lengths; only its selection is stubbed.
    monkeypatch.setattr(learner,'local_minibatch',lambda c,r:(torch.arange(7),7))
    optimizer = torch.optim.SGD(model.parameters(),lr=.1)
    scaler = torch.amp.GradScaler('cpu',enabled=False)
    learner.advance_minibatch(model,optimizer,scaler,cursor,torch.Generator(),0,1,3,
        entropy_coefficient=.005,value_state={'value_calibration':{'completed_updates':20,
        'target':.5,'measured_value_coefficient':.5}})
    logits, value = reference(dict(features=normal))
    policy, entropy = learner.distribution(logits,torch.ones(7,3,dtype=torch.bool))
    log_ratio = policy.log_prob(actions)-old
    ratio = log_ratio.exp()
    actor = -torch.minimum(ratio*advantages,ratio.clamp(.8,1.2)*advantages)
    from train.e4_value_diagnostics import value_coefficient
    c = value_coefficient({'value_calibration':{'completed_updates':20,'target':.5,
                                              'measured_value_coefficient':.5}})
    normal_loss = (actor+c*(value-returns).square()-.005*entropy).mean()
    ce,_ = expert.ce_scalar(reference(dict(features=labels))[0],
                            torch.ones(5,3,dtype=torch.bool),target,1,5,1)
    (normal_loss+ce).backward()
    torch.nn.utils.clip_grad_norm_(reference.parameters(),.5)
    with torch.no_grad():
        for parameter in reference.parameters():
            parameter -= .1*parameter.grad
    for actual, expected in zip(model.parameters(),reference.parameters()):
        assert torch.allclose(actual,expected,atol=2e-7)
    assert int(cursor['totals'][6]) == 7  # Expert rows never enter PPO diagnostics.
    assert cursor['expert_ce_stats'][1] == 5
    assert cursor['optimizer_steps'] == 1


def test_preview_labels_exclude_all_skipped_and_return_scratch_on_yield(monkeypatch):
    search = object.__new__(expert.ExpertSearch)
    search.available = [100,101,102]
    search.failed = {}
    monkeypatch.setattr(expert.NoSLSearch,'predict',lambda *a:([0,1,0],[0.]*3,[0.]*3))
    search.progress_for = lambda slots:{s:[0,0,0,0,expert.COMBAT_PHASE] for s in slots}
    # PHASE index is part of the native progress contract.
    search.progress_for = lambda slots:{s:[expert.COMBAT_PHASE]*9 for s in slots}
    reports = [dict(preview_steps=0,candidates=[0],selected=0),
               dict(preview_steps=12,candidates=[0,1],selected=1),
               dict(preview_steps=0,candidates=[0,1],selected=0,skipped='Runic Dome')]
    search.lookahead = SimpleNamespace(select=lambda m,s:([0,1,0],reports))
    search.predict(None,[0,1,2])
    assert search.labels == {1:dict(action=1,agreement=False,preview_steps=12)}
    def interrupted(*args):
        search.available.pop()
        raise expert.ExpertBudgetEnded()
    search.lookahead.select = interrupted
    with pytest.raises(expert.ExpertBudgetEnded):
        search.predict(None,[0,1,2])
    assert search.available == [100,101,102]


def test_source_pool_ratio_and_late_pool_warmup_are_preserved(monkeypatch):
    obj = object.__new__(expert.ExpertCollector)
    obj.source_collector = SimpleNamespace(pools=[[None]*32,[None]*32,[],[None]*32])
    chosen = [0,44,45,54,55,74,75,89,90,99]
    monkeypatch.setattr(torch,'randint',lambda *a,**k:torch.tensor(chosen))
    obj.seed_rng = torch.Generator()
    fresh, restarts = [], []
    monkeypatch.setattr(learner.NoSLCollector,'fresh',lambda self,slots:fresh.extend(slots))
    obj.restart = lambda pool,slot:restarts.append((pool,slot))
    obj.fresh(list(range(10)))
    assert fresh == [0,1]
    assert restarts == [(0,2),(0,3),(1,4),(1,5),(1,6),(1,7),(3,8),(3,9)]


def test_lane_calibration_uses_measured_extra_and_stays_bounded():
    assert expert.next_lane_count(1,2,100) == 10
    assert expert.next_lane_count(10,40,100) == 5
    assert expert.next_lane_count(1,50,100) == 1
    assert expert.next_lane_count(32,.01,100) == 32


def test_every_fourth_exam_and_exact_seed_set():
    assert not due_combat_comparison(62*3600+1)
    assert due_combat_comparison(64*3600+1)
    assert not due_combat_comparison(66*3600+1)
    assert due_combat_comparison(72*3600+1)
    rows = [dict(seed=i) for i in range(2000000000,2000000256)]
    assert complete_seed_set(rows)
    rows[-1] = rows[0]
    assert not complete_seed_set(rows)


def test_rank_checkpoint_keeps_expert_payload_and_existing_rng(tmp_path,monkeypatch):
    monkeypatch.setattr(learner,'collector_snapshot',lambda c:{'existing':'unchanged'})
    monkeypatch.setattr(learner,'write_rank_checkpoint',
        lambda path,state,save,committed:save(state,path))
    collector = SimpleNamespace(search=SimpleNamespace(device=torch.device('cpu')))
    rng = torch.Generator().manual_seed(19)
    before = rng.get_state().clone()
    payload = dict(lanes=3,preview_rng=torch.Generator().manual_seed(88).get_state(),
                   round=dict(update=1235,ticks=7,records=[dict(action=2,data=torch.tensor([1,2],dtype=torch.uint8))]))
    learner.save_rank(tmp_path/'rank.pt',collector,{'start':32768},(1234,136),rng,
                      {'seconds':12.},expert_state=payload)
    saved = torch.load(tmp_path/'rank.pt',weights_only=False)
    assert saved['update'] == (1234,136)
    assert saved['cursor']['start'] == 32768
    assert saved['expert']['round']['ticks'] == 7
    assert torch.equal(saved['expert']['preview_rng'],payload['preview_rng'])
    assert torch.equal(rng.get_state(),before)


def test_resume_expert_round_does_not_recollect_completed_ticks(monkeypatch):
    obj = object.__new__(expert.ExpertIteration)
    obj.round = dict(update=1235,done=True,ticks=64,records=[{'action':1}])
    obj.collect(None,1235,100.)
    assert obj.round['ticks'] == 64 and len(obj.round['records']) == 1


def test_periodic_hook_and_partial_restore_are_in_correct_scopes():
    import ast
    from pathlib import Path
    source = (Path(__file__).parents[1]/'tools/e4_stage12.py').read_text()
    tree = ast.parse(source)
    main = next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='main')
    nested = {n.name:n for n in main.body if isinstance(n,ast.FunctionDef)}
    stopping_names = {n.id for n in ast.walk(nested['stopping']) if isinstance(n,ast.Name)}
    assert 'label' not in stopping_names and 'periodic_combat_window' not in stopping_names
    evaluate_names = {n.id for n in ast.walk(nested['evaluate']) if isinstance(n,ast.Name)}
    assert {'due_combat_comparison','periodic_combat_window'} <= evaluate_names
    loop = next(n for n in main.body if isinstance(n,ast.While))
    before_loop = main.body[:main.body.index(loop)]
    assert any(isinstance(n,ast.If) and any(isinstance(c,ast.Assign)
        and any(isinstance(t,ast.Name) and t.id=='expert' for t in c.targets)
        for c in ast.walk(n)) for n in before_loop)


def test_native_expert_round_snapshot_restores_without_ppo_mutation(tmp_path,monkeypatch):
    # New native integration, CPU only: no historical performance benchmark.
    from train.model import ModelConfig
    from train.e4_midstart import MidstartCollector
    source_search = learner.NoSLSearch(1,torch.device('cpu'),987,probe_slots=128)
    source = MidstartCollector(source_search,torch.Generator().manual_seed(20),rank=0)
    before = deepcopy(source.state())
    torch.manual_seed(123)
    policy = learner.CLSPolicy(ModelConfig(32,1,1,64)).eval()
    obj = expert.ExpertIteration(source,tmp_path,float('inf'),0)
    # Bound to one new mainline step; native observations must pack and replay.
    obj.round = dict(update=1,ticks=63,records=[],seconds=0.,agreement=0,
                     preview_steps=0,done=False,lanes=1)
    obj.collect(policy,1,100.)
    assert obj.round['ticks'] == 64
    saved = obj.state()
    restored = expert.ExpertIteration(source,tmp_path,float('inf'),0,saved)
    assert restored.collector.seeds == obj.collector.seeds
    assert restored.collector.paths == obj.collector.paths
    assert torch.equal(restored.search.lookahead.rng.get_state(),obj.search.lookahead.rng.get_state())
    assert restored.round['ticks'] == 64
    assert source.seeds == before['seeds'] and source.paths == before['paths']
    assert torch.equal(source.seed_rng.get_state(),before['seed_rng'])


def _ddp_expert_worker(rank, init_file, output):
    import torch.distributed as dist
    from torch.nn.parallel import DistributedDataParallel
    from train import e4_expert_iteration as ex
    from train import e4_stage12 as le
    dist.init_process_group('gloo',init_method='file://'+init_file,rank=rank,world_size=2)
    torch.set_num_threads(1)
    torch.manual_seed(31)
    policy = TinyPolicy()
    model = DistributedDataParallel(policy,broadcast_buffers=False)
    torch.manual_seed(42+rank)
    normal = torch.randn(7,3)
    actions = torch.arange(7)%3
    old = policy(dict(features=normal))[0].log_softmax(-1).detach()[torch.arange(7),actions]
    returns, advantages = torch.randn(7), torch.randn(7)
    torch.manual_seed(60)
    labels = torch.randn(5,3)
    targets = torch.arange(5)%3
    if rank == 0:
        labels,targets = labels[:0],targets[:0]
    fragment = dict(data=normal,offsets=torch.arange(8),actions=actions,logs=old,
        returns=returns,advantages=advantages,policy_mask=torch.ones(7,3,dtype=torch.bool))
    cursor = dict(fragment=fragment,indices=torch.arange(7),counts=[7,7],order=torch.arange(14),
        start=0,epoch=0,finite=torch.tensor(True),totals=torch.zeros(8,dtype=torch.double),
        optimizer_steps=0,optimizer_attempts=0,amp_skipped_steps=0,
        expert=dict(data=labels,offsets=torch.arange(len(labels)+1),actions=targets,
                    policy_mask=torch.ones(len(labels),3,dtype=torch.bool)),
        expert_total=5,expert_minibatches=1,expert_ce_stats=[0.,0.],expert_ce_seconds=0.)
    def packed(data,offsets,ids,device):
        return dict(features=data[ids],candidate_mask=torch.ones(len(ids),3,dtype=torch.bool))
    le.packed_inputs = ex.packed_inputs = packed
    le.local_minibatch = lambda c,r:(torch.arange(7),14)
    optimizer = torch.optim.SGD(model.parameters(),lr=.1)
    le.advance_minibatch(model,optimizer,torch.amp.GradScaler('cpu',enabled=False),cursor,
        torch.Generator(),rank,2,3,entropy_coefficient=.005,
        value_state={'value_calibration':{'completed_updates':20,'target':.5,
                                         'measured_value_coefficient':.5}})
    torch.save(dict(weights=policy.state_dict(),normal=normal,actions=actions,old=old,
                    returns=returns,advantages=advantages,labels=labels,targets=targets),
               output+f'/worker-{rank}.pt')
    dist.destroy_process_group()


def test_real_ddp_zero_label_rank_matches_global_combined_objective(tmp_path):
    import torch.multiprocessing as mp
    mp.start_processes(_ddp_expert_worker,args=(str(tmp_path/'rendezvous'),str(tmp_path)),
                       nprocs=2,join=True,start_method='fork')
    rows = [torch.load(tmp_path/f'worker-{i}.pt',weights_only=False) for i in range(2)]
    torch.manual_seed(31)
    model = TinyPolicy()
    losses = []
    for r in rows:
        logits,value = model(dict(features=r['normal']))
        policy,entropy = learner.distribution(logits,torch.ones(7,3,dtype=torch.bool))
        ratio = (policy.log_prob(r['actions'])-r['old']).exp()
        actor = -torch.minimum(ratio*r['advantages'],ratio.clamp(.8,1.2)*r['advantages'])
        losses.append((actor+.5*(value-r['returns']).square()-.005*entropy).mean())
    ce,_ = expert.ce_scalar(model(dict(features=rows[1]['labels']))[0],
        torch.ones(5,3,dtype=torch.bool),rows[1]['targets'],1,5,1)
    ((losses[0]+losses[1])/2+ce).backward()
    torch.nn.utils.clip_grad_norm_(model.parameters(),.5)
    with torch.no_grad():
        for p in model.parameters():p -= .1*p.grad
    for row in rows:
        for key,value in model.state_dict().items():
            assert torch.allclose(row['weights'][key],value,atol=3e-7)
