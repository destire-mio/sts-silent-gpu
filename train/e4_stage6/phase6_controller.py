from copy import deepcopy
import time

import torch
import torch.distributed as dist


def run_training_slice(worker, target_node_seconds, allocation_deadline, stop_file, previous_boundary_node_seconds):
    world,device=worker.binding['world'],worker.device
    started_nodes=worker.state['training_node_seconds']
    boundary=previous_boundary_node_seconds
    while True:
        flag=torch.tensor(int(stop_file.exists() or time.monotonic()>=allocation_deadline),device=device)
        dist.all_reduce(flag,op=dist.ReduceOp.MAX)
        if bool(flag):
            return dict(finished=False,target_node_seconds=target_node_seconds,
                actual_node_seconds=worker.state['training_node_seconds'],started_node_seconds=started_nodes,
                boundary_node_seconds=boundary,reason='parent_stop_or_allocation_deadline')
        actual=worker.state['training_node_seconds']
        if actual>=target_node_seconds:
            assert actual-target_node_seconds<=boundary+1e-6
            return dict(finished=True,target_node_seconds=target_node_seconds,actual_node_seconds=actual,
                started_node_seconds=started_nodes,boundary_node_seconds=boundary,
                signed_error_node_seconds=actual-target_node_seconds,reason='target_boundary')
        now=time.monotonic()
        deadline=torch.tensor(min(allocation_deadline,now+(target_node_seconds-actual)*8/world),
                              dtype=torch.float64,device=device)
        dist.all_reduce(deadline,op=dist.ReduceOp.MIN)
        call_started=time.monotonic()
        result=worker.step(float(deadline))
        call_seconds=time.monotonic()-call_started
        if result['action'] in ('training_health_stop','B_entropy_below_half_target'):
            return dict(finished=False,target_node_seconds=target_node_seconds,
                actual_node_seconds=worker.state['training_node_seconds'],started_node_seconds=started_nodes,
                boundary_node_seconds=boundary,reason=result['action'])
        if not result['performed_training']:
            flag=torch.tensor(int(stop_file.exists() or time.monotonic()>=allocation_deadline),device=device)
            dist.all_reduce(flag,op=dist.ReduceOp.MAX)
            if bool(flag):
                continue
            boundary=call_seconds*world/8
            bound=torch.tensor(boundary,dtype=torch.float64,device=device)
            dist.all_reduce(bound,op=dist.ReduceOp.MAX)
            actual=worker.state['training_node_seconds']
            assert abs(actual-target_node_seconds)<=float(bound)+1e-6
            return dict(finished=True,target_node_seconds=target_node_seconds,actual_node_seconds=actual,
                started_node_seconds=started_nodes,boundary_node_seconds=float(bound),
                signed_error_node_seconds=actual-target_node_seconds,reason='target_boundary_precheck')
        bound=torch.tensor(result['atomic_overshoot_seconds']*world/8,dtype=torch.float64,device=device)
        dist.all_reduce(bound,op=dist.ReduceOp.MAX)
        boundary=float(bound)


class Phase6Schedule:
    def __init__(self,binding):
        assert binding['world'] in (7,8)
        self.state=dict(format=1,binding=deepcopy(binding),phase='plan',round=0,
            paired_reference=0.,nominal_target=0.,common_final_target=7200.,round_is_final=False,
            B_target=None,actual_progress={'A':0.,'B':0.},checkpoints={},last_matched=None,
            last_training_reports={},examinations=[],full_runs={},unmatched_training_expense_retained=False)

    def restore(self,saved):
        assert saved['format']==1 and saved['binding']==self.state['binding']
        self.state=deepcopy(saved)

    def next_action(self,remaining_node_seconds,examination_reserve_remaining,overhead_reserve):
        assert min(remaining_node_seconds,examination_reserve_remaining,overhead_reserve)>=0
        s=self.state
        if s['phase']=='plan':
            common=min(7200.,max(0.,(remaining_node_seconds-examination_reserve_remaining-
                overhead_reserve+2*s['paired_reference'])/2))
            s['common_final_target']=common
            if common<=s['paired_reference']:
                s['phase']='final_A' if s['last_matched'] is not None else 'no_balanced_training'
            else:
                s['nominal_target']=min(common,s['paired_reference']+1800*s['binding']['world']/8)
                s['round_is_final']=s['nominal_target']==common
                s['phase']='train_A'
        if s['phase'] in ('train_A','train_B'):
            arm=s['phase'][-1]
            target=s['nominal_target'] if arm=='A' else s['B_target']
            needed=max(0.,target-s['actual_progress'][arm])
            if arm=='A':needed+=max(0.,target-s['actual_progress']['B'])
            if needed+examination_reserve_remaining+overhead_reserve>remaining_node_seconds:
                s['unmatched_training_expense_retained']=True
                s['phase']='restore_matched_pair' if s['last_matched'] is not None else 'no_balanced_training'
            else:
                return dict(action='train',arm=arm,target_node_seconds=target,round=s['round'])
        if s['phase'] in ('exam_A','exam_B'):
            arm=s['phase'][-1]
            return dict(action='examine',arm=arm,checkpoint=s['checkpoints'][arm],round=s['round'])
        if s['phase'] in ('final_A','final_B'):
            arm=s['phase'][-1]
            return dict(action='full_run_1024',arm=arm,checkpoint=s['last_matched']['checkpoints'][arm],
                        round=s['last_matched']['round'])
        if s['phase']=='restore_matched_pair':
            return dict(action='select_last_matched_for_evaluation',matched=deepcopy(s['last_matched']),
                        actual_progress=deepcopy(s['actual_progress']),unmatched_training_expense_retained=True)
        assert s['phase'] in ('complete','no_balanced_training')
        return dict(action=s['phase'])

    def record_training(self,arm,report,checkpoint):
        s=self.state;assert s['phase']=='train_'+arm
        target=s['nominal_target'] if arm=='A' else s['B_target']
        assert report['target_node_seconds']==target
        assert report['actual_node_seconds']>=s['actual_progress'][arm]
        s['actual_progress'][arm]=report['actual_node_seconds']
        s['last_training_reports'][arm]=deepcopy(report);s['checkpoints'][arm]=checkpoint
        if not report['finished']:return
        assert abs(report['actual_node_seconds']-target)<=report['boundary_node_seconds']+1e-6
        if arm=='A':
            s['B_target']=report['actual_node_seconds']
            s['phase']='train_B'
        else:
            a=s['actual_progress']['A'];b=s['actual_progress']['B']
            assert abs(b-a)<=report['boundary_node_seconds']+1e-6
            s['current_pair']=dict(reference_target=a,actual_A=a,actual_B=b,signed_difference_B_minus_A=b-a,
                B_boundary_node_seconds=report['boundary_node_seconds'],
                matching_scope='B targets actual A time; both raw times and atomic stop discrepancy retained, not rounded to identical counters')
            s['phase']='exam_A'

    def record_examination(self,arm,record):
        s=self.state;assert s['phase']=='exam_'+arm and record['complete']
        assert record['checkpoint']==s['checkpoints'][arm]
        s['examinations'].append(dict(record,arm=arm,round=s['round']))
        if arm=='A':s['phase']='exam_B'
        else:
            s['last_matched']=dict(round=s['round'],checkpoints=deepcopy(s['checkpoints']),
                                  time=deepcopy(s['current_pair']))
            s['paired_reference']=s['current_pair']['reference_target']
            s['round']+=1
            s['phase']='final_A' if s['round_is_final'] else 'plan'

    def select_last_matched(self):
        s=self.state;assert s['phase']=='restore_matched_pair' and s['last_matched'] is not None
        s['phase']='final_A'

    def record_full_run(self,arm,record):
        s=self.state;assert s['phase']=='final_'+arm and arm not in s['full_runs']
        assert record['complete'] and record['episodes']==1024
        assert record['checkpoint']==s['last_matched']['checkpoints'][arm]
        s['full_runs'][arm]=deepcopy(record)
        s['phase']='final_B' if arm=='A' else 'complete'
