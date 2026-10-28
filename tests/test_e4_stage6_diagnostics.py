from copy import deepcopy
from dataclasses import asdict
from types import SimpleNamespace
import time

import pytest
import torch

from train.model import ModelConfig, Policy
from train.e4_stage6.diagnostics import b_round_diagnostics, b_diagnostic_stop
from train.e4_stage6.phase6_runtime import Phase6Runtime
from train.e4_stage6.phase6_storage import file_digest
from train.e4_stage6.combat import CombatPolicy


def row(update, seconds, entropy, copies, kl, moments):
    return dict(update=update,training_node_seconds=seconds,H_imit=entropy,
        imitation_entropy_samples=copies,beta_used=.03+update*.001,copy_samples=copies*4,
        copy_kl=kl,copy_clip_fraction=.2,combat_value_loss=.4,
        copy_advantage_moments_global=moments,copy_advantage_std_global=1.)


def test_round_metrics_use_samples_and_raw_population_moments_not_mean_of_std():
    rows=[row(1,1.,.5,1,.02,[1.,1.,1.]),row(2,2.,.9,3,.10,[9.,35.,3.])]
    report=b_round_diagnostics(rows,0,0.,2.,.67)
    assert report['H_imit']==pytest.approx(.8)
    assert report['copy_kl']==pytest.approx(.08)
    assert report['beta_used']==pytest.approx(.032)
    assert report['copy_advantage_std_global']==pytest.approx(torch.tensor([1.,1.,3.,5.]).std(unbiased=False).item())
    assert report['copy_advantage_moments_global']==[10.,36.,4.]
    assert len(report['updates'])==2 and report['copy_advantage_moments_complete']
    legacy=b_round_diagnostics([row(1,1.,.5,1,.02,None)],0,0.,2.,.67)
    assert legacy['copy_advantage_std_global']is None and not legacy['copy_advantage_moments_complete']
    assert legacy['H_imit']==.5
    assert b_round_diagnostics(rows,1,2.,3.,.67)['H_imit']is None


def test_copy_kl_stop_requires_two_adjacent_rounds_and_strict_thresholds():
    report=dict(round=0,H_imit=.5,entropy_target=1.,copy_kl=.06)
    assert b_diagnostic_stop([report])is None
    assert b_diagnostic_stop([report,dict(report,round=1)])=='B_copy_kl_above_0_05_two_rounds'
    assert b_diagnostic_stop([report,dict(report,round=2)])is None
    assert b_diagnostic_stop([report,dict(report,round=1,copy_kl=.05)])is None
    assert b_diagnostic_stop([dict(report,H_imit=.499)])=='B_entropy_below_half_target'
    assert b_diagnostic_stop([dict(report,H_imit=None,copy_kl=None)])is None


def test_original_source_exam_resumes_separately_once_without_training(tmp_path, monkeypatch):
    runtime=object.__new__(Phase6Runtime)
    runtime.root,runtime.rank,runtime.device=tmp_path,0,torch.device('cpu')
    runtime.worker=None
    runtime.binding=dict(source_hashes=['source']*8)
    runtime.schedule=SimpleNamespace(state={'phase':'train_A','round':1})
    runtime.state=dict(training={'A':'original-A','B':'original-B'},evaluations={},stopped_reason=None)
    policy=Policy(ModelConfig(16,1,2,32))
    source=dict(commit='67735d2808018721eb318d2c954de6954eeb314a',task='rl',world_size=8,
        config=asdict(policy.config),model=deepcopy(policy.state_dict()),optimizer={},ppo_rng={},
        state=dict(phase='evaluation',updates=8,imitation_beta=.03,imitation_entropy_target=.67,
                   training_rng={},train_state=dict(seed_rng=None,search_rng=None)))
    runtime.source_path=tmp_path/'original.pt';torch.save(source,runtime.source_path)
    original=runtime.source_path.read_bytes();runtime.source_sha256=file_digest(runtime.source_path)
    snapshots={};saved_states=[];calls=[]
    def save_worker(worker,kind,round_number):
        assert kind=='evaluation'and round_number==-1
        reference=dict(directory=f'baseline-{len(snapshots)}',manifest_sha256='baseline')
        snapshots[reference['directory']]=deepcopy(worker.state)
        return reference
    runtime.store=SimpleNamespace(save_worker=save_worker,save_state=lambda s:saved_states.append(deepcopy(s)))
    def evaluator(arm,loaded,checkpoint_hash,reference):
        assert arm=='A'and checkpoint_hash==runtime.source_sha256
        assert all(torch.equal(value,loaded.state_dict()[name])for name,value in source['model'].items())
        state=deepcopy(snapshots[reference['directory']])if reference else dict(phase='examination',progress=0)
        runtime.worker=SimpleNamespace(state=state)
        calls.append(reference)
        return runtime.worker
    runtime.evaluation_worker=evaluator
    def advance(worker,target,deadline):
        assert target=='examination_complete'
        worker.state['progress']+=1
        if worker.state['progress']==2:worker.state['phase']=target
        return worker.state['phase']==target
    runtime.advance_evaluation=advance
    runtime.write_examination=lambda worker,action,reference:dict(complete=True,checkpoint=action['checkpoint'],reference=reference,evaluation_only=True,rows=7450)
    monkeypatch.setattr(torch.optim,'Adam',lambda *a,**k:pytest.fail('Baseline must not construct a trainer/optimizer'))
    assert not runtime.source_examination(time.monotonic()+30)
    assert runtime.state['source_examination']['complete']is False
    assert runtime.source_examination(time.monotonic()+30)
    assert calls[1]is not None and len(calls)==2
    assert runtime.state['training']=={'A':'original-A','B':'original-B'}and runtime.state['evaluations']=={}
    assert any(not s['source_examination']['complete']for s in saved_states)
    assert runtime.state['source_examination']['checkpoint']['additional_training_updates']==0
    runtime.source=lambda:pytest.fail('Completed baseline must never rerun')
    assert runtime.source_examination(time.monotonic()+30)
    assert len(calls)==2 and runtime.source_path.read_bytes()==original


def test_interrupted_source_exam_precedes_and_prevents_training(monkeypatch,tmp_path):
    runtime=object.__new__(Phase6Runtime)
    runtime.device,runtime.world=torch.device('cpu'),7
    runtime.state=dict(stopped_reason=None,evaluation_node_seconds=0.)
    runtime.source_examination=lambda deadline:False
    runtime.save=lambda:None
    runtime.schedule=SimpleNamespace(next_action=lambda *a:pytest.fail('No training before baseline completes'))
    monkeypatch.setattr('torch.distributed.all_reduce',lambda *a,**k:None)
    result=runtime.run(SimpleNamespace(deadline=time.monotonic()+30),3600,210)
    assert result==dict(reason='source_examination_interrupted',complete=False)
    assert runtime.state['evaluation_node_seconds']>=0


def test_runtime_stops_after_second_high_kl_round_and_keeps_checkpoint(tmp_path,monkeypatch):
    runtime=object.__new__(Phase6Runtime)
    runtime.root,runtime.rank=tmp_path,0
    worker=SimpleNamespace(state=dict(rows=[row(1,1.,.7,1,.06,None),row(2,3.,.7,1,.07,[1.,1.,1.])],entropy_target=.67))
    runtime.worker=worker
    runtime.trainer=lambda arm:worker
    runtime.state=dict(training={},boundary={'B':0.},stopped_reason=None)
    runtime.schedule=SimpleNamespace(state=dict(last_matched=dict(round=0,time=dict(actual_B=2.))),record_training=lambda *a:None)
    runtime.store=SimpleNamespace(save_worker=lambda *a:dict(directory='B-round1',manifest_sha256='kept'))
    runtime.stop_file=tmp_path/'stop'
    runtime.save=lambda:None
    monkeypatch.setattr('train.e4_stage6.phase6_runtime.run_training_slice',lambda *a:dict(
        finished=True,target_node_seconds=4.,actual_node_seconds=4.,boundary_node_seconds=0.,reason='target_boundary'))
    assert runtime.train(dict(arm='B',round=1,target_node_seconds=4.),time.monotonic()+30)is False
    assert runtime.state['stopped_reason']=='B_copy_kl_above_0_05_two_rounds'
    assert runtime.state['training']['B']['directory']=='B-round1'
    assert [r['round']for r in runtime.state['B_diagnostics']]==[0,1]
    assert runtime.state['B_diagnostics'][1]['copy_kl']==.07


def test_original_round0_B_report_is_available_when_budget_selects_old_pair(tmp_path):
    runtime=object.__new__(Phase6Runtime)
    runtime.root,runtime.rank,runtime.device=tmp_path,0,torch.device('cpu')
    runtime.worker=None;runtime.state={}
    policy=CombatPolicy(ModelConfig(16,1,2,32))
    saved=dict(binding=dict(config=asdict(policy.config)),model=policy.state_dict(),
        state=dict(rows=[row(1,1.,.7,2,.06,None)],training_node_seconds=2.,entropy_target=.67))
    runtime.store=SimpleNamespace(load_worker=lambda reference:saved)
    runtime.evaluation_worker=lambda arm,loaded,digest,reference:loaded
    loaded=runtime.evaluator('B',dict(round=0,manifest_sha256='original'))
    assert all(torch.equal(value,loaded.state_dict()[name])for name,value in policy.state_dict().items())
    assert runtime.state['B_diagnostics'][0]['copy_kl']==.06
    assert runtime.state['B_diagnostics'][0]['copy_advantage_std_global']is None
