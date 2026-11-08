from copy import deepcopy
import math
import pytest
import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel
from train.e4_stage12 import CLSPolicy,NoSLSearch,NoSLCollector,new_cursor,advance_minibatch,finish_metrics
from train.e4_value_diagnostics import gradient_measurement,value_coefficient,explained_variance,nonforced_entropy
from train.e4_evaluation_ledger import visible_state,spearman,value_correlations,EvaluationLedgerCollector
from train.model import ModelConfig,network_inputs,distribution


def test_calibration_preserves_gradients_and_ddp_optimizer(tmp_path):
    torch.set_num_threads(1)
    torch.manual_seed(133)
    dist.init_process_group('gloo',init_method=(tmp_path/'gloo').as_uri(),rank=0,world_size=1)
    try:
        policy=CLSPolicy(ModelConfig(16,1,2,32))
        model=DistributedDataParallel(policy)
        collector=NoSLCollector(NoSLSearch(2,torch.device('cpu'),133),torch.Generator().manual_seed(133))
        fragment=collector.collect(policy,length=2)
        inputs=network_inputs(collector.search.arrays,torch.device('cpu'),torch.tensor([0,1]))
        with torch.no_grad():
            logits,value=policy(inputs)
            categorical,_=distribution(logits,inputs['candidate_mask'])
            actions=categorical.probs.argmax(-1)
            old=categorical.log_prob(actions)
        weights={k:v.clone() for k,v in policy.state_dict().items()}
        with model.no_sync():
            measured=gradient_measurement(policy,inputs,actions,old,torch.tensor([1.,-1.]),value+torch.tensor([.1,.2]),scale=8.)
        assert all(p.grad is None for p in policy.parameters())
        assert all(torch.equal(v,policy.state_dict()[k]) for k,v in weights.items())
        assert measured['actor_encoder_norm'] > 0 and measured['weighted_value_encoder_norm'] > 0
        assert measured['target'] == pytest.approx(min(200.,max(.5,.25/measured['ratio'])))
        state={'value_calibration':dict(measured,target=10.)}
        for update,expected in [(0,.5),(10,5.25),(20,10.),(23,10.)]:
            state['value_calibration']['completed_updates']=update
            assert value_coefficient(state)==expected
        state={}
        rng=torch.Generator().manual_seed(133)
        cursor=new_cursor(fragment,rng,1)
        optimizer=torch.optim.Adam(model.parameters(),lr=1e-4)
        scaler=torch.amp.GradScaler('cpu',enabled=False)
        logs=[]
        advance_minibatch(model,optimizer,scaler,cursor,rng,0,1,microbatch=2,value_state=state,diagnostic_log=logs.append)
        advance_minibatch(model,optimizer,scaler,cursor,rng,0,1,microbatch=2,value_state=state,diagnostic_log=logs.append)
        row=finish_metrics(cursor,model,scaler)
        assert len(logs)==1 and row['optimizer_steps']==2 and row['finite']
        assert math.isfinite(row['gradient_norm_before_clip_max']) and 0 <= row['gradient_clipped_fraction'] <= 1
        state.update(update=40, claude171_value_calibration_pending=True)
        inherited = value_coefficient(state)
        cursor = new_cursor(fragment, rng, 1)
        advance_minibatch(model, optimizer, scaler, cursor, rng, 0, 1, microbatch=2,
                          value_state=state, diagnostic_log=logs.append)
        assert logs[-1]['event'] == 'claude171_value_recalibrated'
        assert logs[-1]['start_update'] == 41 and logs[-1]['old_coefficient'] == inherited
        assert value_coefficient(state) == inherited
        assert 'claude171_value_calibration_pending' not in state
        advance_minibatch(model, optimizer, scaler, cursor, rng, 0, 1, microbatch=2,
                          value_state=state, diagnostic_log=logs.append)
        assert len(logs) == 2
        fragment.update(returns=torch.tensor([0.,1.,2.,3.]),values=torch.tensor([0.,1.,2.,3.]),action_valid=torch.ones(4,dtype=torch.bool),decision_types=torch.tensor([0,0,1,1]))
        ev=explained_variance(fragment)
        assert ev[0]['explained_variance']==1. and ev[1]['samples']==2 and ev[3]['explained_variance'] is None
        totals=torch.tensor([[1.,0.],[2.,0.],[2.,3.]])
        entropy=nonforced_entropy(totals,['combat','map'])
        assert entropy[0]['normalized_entropy_nonforced']==.5 and entropy[0]['forced_fraction']==.5
        assert entropy[1]['normalized_entropy_nonforced'] is None and entropy[1]['forced_fraction']==1.
    finally:
        dist.destroy_process_group()


def test_visible_evaluation_ledger_and_tied_spearman():
    torch.set_num_threads(1)
    search=NoSLSearch(1,torch.device('cpu'),133,greedy=True)
    collector=EvaluationLedgerCollector(search,torch.Generator().manual_seed(133))
    collector.fresh([0])
    raw=search.batch.state_bytes(0)
    state=visible_state(search,0)
    assert len(state['deck'])==13 and 0 < state['hp'] <= state['max_hp'] and 'Ring of the Snake' in state['relics']
    assert all(isinstance(c['upgraded'],bool) and c['card_id'] for c in state['deck'])
    assert search.batch.state_bytes(0)==raw
    policy=CLSPolicy(ModelConfig(16,1,2,32))
    progress=search.progress_for([0])
    actions,*_=collector.choose(policy,[0],progress)
    collector.advance([0],actions,progress)
    assert collector.records[0]['counts']['card_removals']==0
    assert spearman([(1.,1),(2.,2),(2.,2),(3.,3)])==pytest.approx(1.)
    assert spearman([(1.,1),(2.,2),(3.,1)])==pytest.approx(0.)
    assert spearman([(1.,1),(1.,2)]) is None
    result=value_correlations([dict(act1_boss_value=.2,final_floor=10),dict(act1_boss_value=.8,final_floor=20)])
    assert result['act1_boss_value']==dict(cases=2,spearman=1.)


def test_768_ppo_ramp_hits_target_in_round_20_and_preserves_milestones():
    from train.e4_value_diagnostics import record_ppo_calibration_round

    state = dict(update=100, training_seconds=1000,
                 value_calibration=dict(start_coefficient=181., target=29., completed_updates=0),
                 claude171_value_calibration=dict(start_update=101, old_coefficient=181.,
                                                 new_coefficient=29., ratio=.25/29, milestones={}))
    events = []
    for round_number in range(1, 23):
        row = dict(value_loss_coefficient=value_coefficient(state), value_loss=round_number / 100.)
        state['update'] += 1
        state['value_calibration']['completed_updates'] += 1
        event = record_ppo_calibration_round(state, row)
        if event is not None:
            events.append(event)
        if round_number == 10:
            state = deepcopy(state)
    report = events[-1]
    assert report['event'] == 'claude171_value_calibration_complete'
    assert len(events) == 3 and set(report['milestones']) == {'1', '10', '20'}
    for key, expected in [('1', 181.), ('10', 109.), ('20', 29.)]:
        milestone = report['milestones'][key]
        assert milestone['coefficient'] == expected
        assert milestone['value_mse'] == int(key) / 100.
    assert value_coefficient(state) == 29.



def test_768_calibration_retries_forced_rows_and_keeps_manual_override(tmp_path):
    torch.set_num_threads(1)
    torch.manual_seed(179)
    dist.init_process_group("gloo", init_method=(tmp_path / "gloo179").as_uri(), rank=0, world_size=1)
    try:
        policy = CLSPolicy(ModelConfig(16, 1, 2, 32))
        model = DistributedDataParallel(policy)
        device = torch.device("cpu")
        collector = NoSLCollector(NoSLSearch(2, device, 133), torch.Generator().manual_seed(133))
        fragment = collector.collect(policy, length=2)
        forced = dict(fragment, policy_mask=torch.zeros_like(fragment["policy_mask"]),
                      logs=torch.zeros_like(fragment["logs"]))
        forced["policy_mask"].scatter_(1, fragment["actions"][:, None], True)
        optimizer = torch.optim.Adam(model.parameters(), lr=0.)
        scaler = torch.amp.GradScaler("cpu", enabled=False)
        rng = torch.Generator().manual_seed(179)
        state = dict(update=62, claude171_value_calibration_pending=True)
        logs = []
        advance_minibatch(model, optimizer, scaler, new_cursor(forced, rng, 1), rng, 0, 1, microbatch=2,
                          value_state=state, diagnostic_log=logs.append)
        assert logs[-1]["event"] == "claude171_value_calibration_deferred"
        assert state["claude171_value_calibration_pending"]
        assert "value_calibration" not in state
        advance_minibatch(model, optimizer, scaler, new_cursor(fragment, rng, 1), rng, 0, 1, microbatch=2,
                          value_state=state, diagnostic_log=logs.append)
        assert logs[-1]["event"] == "claude171_value_recalibrated"
        assert logs[-1]["actor_encoder_norm"] > 0 and math.isfinite(logs[-1]["ratio"])
        assert "claude171_value_calibration_pending" not in state
        state["standalone_value_coefficient"] = 181.
        state["claude171_value_calibration_pending"] = True
        advance_minibatch(model, optimizer, scaler, new_cursor(dict(fragment), rng, 1), rng, 0, 1, microbatch=2,
                          value_state=state, diagnostic_log=logs.append)
        assert value_coefficient(state) == 181.
        assert len(logs) == 2
    finally:
        dist.destroy_process_group()
