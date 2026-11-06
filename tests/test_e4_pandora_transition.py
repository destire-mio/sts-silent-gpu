import random
import subprocess
import xml.etree.ElementTree as ET

import numpy as np
import pytest
import torch

from train.e4_midstart import MidstartCollector
from train.e4_stage12 import (NoSLSearch, collector_snapshot, restart_rank_engine,
                             restore_rank, pandora_cuda_gates)


def collector(seed):
    return MidstartCollector(NoSLSearch(2,torch.device('cpu'),seed,storage_slots=1024),
        torch.Generator().manual_seed(seed+1),rank=0)


def test_engine_restart_never_replays_old_lanes_or_roots_and_retains_rng():
    torch.set_num_threads(1)
    source = collector(153)
    source.fresh([0,1])
    for pool in range(4): source.retain(pool,pool%2)
    source.decisions = 123
    snapshot = collector_snapshot(source)
    snapshot['paths'] = [[999],[999]]
    for pool in snapshot['midstart']['pools']:
        pool[0]['path'] = [999]
    expected_rng = torch.Generator().set_state(snapshot['seed_rng'])
    expected_seeds = torch.randint(1000000000,2000000000,(2,),generator=expected_rng).tolist()
    ppo_rng = torch.Generator().manual_seed(54)
    old = dict(collector=snapshot,cursor=dict(optimizer_steps=22),
        round_state=dict(ready={'old_fragment':True},seconds=17.),
        ppo_rng=ppo_rng.get_state(),torch_rng=torch.get_rng_state(),
        python_rng=random.getstate(),numpy_rng=np.random.get_state(),update=(1830,201))
    target = collector(8)
    changed = restart_rank_engine(old,target)
    assert old['cursor']['optimizer_steps'] == 22 and old['collector']['paths'] == [[999],[999]]
    assert changed['cursor'] is None and changed['round_state']['ready'] is None
    restored_rng = torch.Generator()
    cursor,identity,round_state = restore_rank(changed,target,torch.device('cpu'),restored_rng)
    assert cursor is None and identity == (1830,201)
    assert target.seeds == [None,None] and target.paths == [[],[]]
    assert target.pools == [[],[],[],[]] and target.randomizations == [[],[]]
    assert target.partial is None and target.decisions == 123
    assert torch.equal(target.seed_rng.get_state(),snapshot['seed_rng'])
    assert torch.equal(target.search.rng.get_state(),snapshot['search_rng'])
    assert torch.equal(restored_rng.get_state(),old['ppo_rng'])
    assert torch.equal(torch.get_rng_state(),old['torch_rng'])
    assert random.getstate() == old['python_rng']
    np.testing.assert_array_equal(np.random.get_state()[1],old['numpy_rng'][1])
    target.fresh([0,1])
    assert target.seeds == expected_seeds and target.starts == ['fresh','fresh']
    assert target.pools == [[],[],[],[]]


def gate_xml(path, skipped=False, missing=False):
    root = ET.Element('testsuite')
    names = ['test_cuda_dispatch','test_cuda_effect_fields','test_cuda_faults',
             'test_cuda_parity','test_cuda_root_cache']
    for name in (names[:-1] if missing else names):
        case = ET.SubElement(root,'testcase',name=name,status='run')
        if skipped and name == names[0]: ET.SubElement(case,'skipped')
    ET.ElementTree(root).write(path)


def test_cuda_gate_requires_exact_five_run_not_skipped(tmp_path,monkeypatch):
    receipt = tmp_path/'gates.xml'
    def run(args,check):
        assert check and args[args.index('-R')+1] == (
            '^(test_cuda_dispatch|test_cuda_effect_fields|test_cuda_faults|test_cuda_parity|test_cuda_root_cache)$')
        assert args[-1] == str(receipt)
        gate_xml(receipt)
    monkeypatch.setattr(subprocess,'run',run)
    pandora_cuda_gates(tmp_path,receipt)
    for skipped,missing in [(True,False),(False,True)]:
        monkeypatch.setattr(subprocess,'run',lambda args,check:gate_xml(receipt,skipped,missing))
        with pytest.raises(AssertionError): pandora_cuda_gates(tmp_path,receipt)


def test_engine_marker_is_persisted_and_prevents_second_startup(tmp_path):
    import ast
    from pathlib import Path
    tree = ast.parse((Path(__file__).resolve().parents[1]/'tools/e4_stage12.py').read_text())
    assignment = next(node for node in ast.walk(tree) if isinstance(node,ast.Assign)
        and any(isinstance(target,ast.Name) and target.id == 'engine_transition' for target in node.targets))
    expr = compile(ast.Expression(assignment.value),'<engine-switch>','eval')
    state = dict(update=1830,training_seconds=296100.,claude150_dev78_rollback={'checkpoint':'dev-78.pt'})
    assert eval(expr,dict(saved={'state':state}))
    marker = next(node for node in ast.walk(tree) if isinstance(node,ast.Assign)
        and any(isinstance(target,ast.Subscript) and isinstance(target.slice,ast.Constant)
                and target.slice.value == 'claude153_pandora_grid' for target in node.targets))
    exec(compile(ast.Module(body=[marker],type_ignores=[]),'<marker>','exec'),dict(state=state))
    path = tmp_path/'marker.pt'
    torch.save(state,path)
    restored = torch.load(path,weights_only=False)
    assert not eval(expr,dict(saved={'state':restored}))
    assert restored['claude150_dev78_rollback'] == {'checkpoint':'dev-78.pt'}
    assert restored['update'] == 1830 and restored['training_seconds'] == 296100.


def test_canonical_engine_transition_accounts_applied_partial_once():
    import ast
    from pathlib import Path
    source = (Path(__file__).resolve().parents[1]/'tools/e4_stage12_chain.py').read_text()
    tree = ast.parse(source)
    block = next(node for node in ast.walk(tree) if isinstance(node,ast.If)
                 and "'claude153_pandora_grid' not in" in ast.unparse(node.test))
    compiled = compile(ast.Module(body=[block],type_ignores=[]),'<engine-partial>','exec')
    for transitioned in (False,True):
        state = dict(world=4,claude150_dev78_rollback={})
        if transitioned: state['claude153_pandora_grid'] = {}
        checkpoint = dict(state=state,optimizer_steps=109926,partial_steps=22,
                          partial_attempts=22,partial_skips=0)
        exec(compiled,dict(checkpoint=checkpoint,state={'jobs':{'42':{'world':4}}},identity='42'))
        assert checkpoint['optimizer_steps'] == 109926
        if transitioned:
            assert checkpoint['partial_steps'] == 22 and 'retained_partial_optimizer_steps' not in checkpoint
        else:
            assert checkpoint['retained_partial_optimizer_steps'] == 22
            assert checkpoint['partial_steps'] == checkpoint['partial_attempts'] == checkpoint['partial_skips'] == 0
