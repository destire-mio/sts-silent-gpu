"""Initialize the requested student while retaining the teacher-era environment states."""
import argparse
from dataclasses import asdict
import json
from pathlib import Path
import torch
from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig
from train.e4_value_diagnostics import value_coefficient


def prepare(source, output, teacher, width, small_seconds):
    old = torch.load(source/'model.pt', map_location='cpu', weights_only=False)
    assert old['state']['stop_reason'] in (None, 'allocation_end_or_yield')
    output.mkdir(parents=True, exist_ok=True)
    if output != source:
        assert not (output/'model.pt').exists()
    config = ModelConfig(512,8,8,2048) if width == 512 else ModelConfig(width,6,6 if width == 384 else 5,4*width)
    torch.manual_seed(20261101)
    policy = CLSPolicy(config)
    optimizer = torch.optim.Adam(policy.parameters(), lr=1e-4, eps=1e-5)
    state = dict(update=0, training_seconds=old['state']['training_seconds'], initial_entropy=None,
        next_evaluation=old['state']['next_evaluation'], precision='encoder_fp16_heads_fp32',
        microbatch=512, checkpoint_serial=old['state']['checkpoint_serial']+1, world=8,
        stop_reason=None, high_kl_streak=0, learning_rate=1e-4, model_config=asdict(config),
        teacher_path=str(teacher), distillation_updates=0, distillation_evaluated=False,
        entropy_coefficient=.005, small_round_seconds=small_seconds,
        source_checkpoint=str(source/'model.pt'), source_update=old['state']['update'])
    if width == 512:
        assert source != output
        state['value_calibration'] = dict(target=value_coefficient(old['state']),completed_updates=20)
        assert old['state']['model_config'] == dict(width=384,layers=6,heads=6,feedforward=1536)
        evaluations = [json.loads(p.read_text()) for p in source.glob('evaluation-*.json')]
        eligible = [e for e in evaluations if e.get('complete') and e.get('selection') == 'argmax'
                    and e.get('model_snapshot', '').startswith('dev-')]
        best = max(eligible, key=lambda e:(e['mean_floor'], e['training_seconds']))
        teacher = source / best['model_snapshot']
        teacher_policy = CLSPolicy(ModelConfig(384,6,6,1536))
        teacher_policy.load_state_dict(torch.load(teacher,map_location='cpu',weights_only=True),strict=True)
        state.update(teacher_model_config=dict(width=384,layers=6,heads=6,feedforward=1536),
                     teacher_path=str(teacher),teacher_label='teacher-best384-argmax',
                     teacher_evaluated_model=teacher.name,distillation_target=40)
    identity = (0,state['checkpoint_serial'])
    for rank in range(8):
        record = torch.load(source/f'rank-{rank}.pt', map_location='cpu', weights_only=False)
        assert record['update'] == (old['state']['update'],old['state']['checkpoint_serial'])
        record['update'] = identity
        record['cursor'] = None
        record['collector']['partial'] = None
        record['collector']['completed'] = []
        record['round_state'] = dict(ready=None,seconds=0.,decisions_start=record['collector']['decisions'])
        torch.save(record,output/f'rank-{rank}.pt')
    temporary = output/'model.pt.tmp'
    torch.save(dict(model=policy.state_dict(),optimizer=optimizer.state_dict(),scaler=old['scaler'],
        state=state,ppo_rng=old['ppo_rng']),temporary)
    temporary.replace(output/'model.pt')
    report = dict(event='student_prepared', parameters=sum(p.numel() for p in policy.parameters()),
        model=asdict(config), teacher=str(teacher), source_checkpoint=str(source/'model.pt'),
        source_update=old['state']['update'], state=state, collectors=8, optimizer_steps=0,
        unfinished_small_learner='retained in source backup; fresh teacher collection for student')
    (output/f'preparation-width{width}.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--teacher',type=Path,required=True)
    parser.add_argument('--width',type=int,choices=(384,320,512),default=384)
    parser.add_argument('--small-seconds',type=float,required=True)
    args=parser.parse_args()
    torch.set_num_threads(1)
    print(json.dumps(prepare(args.source,args.output,args.teacher,args.width,args.small_seconds)),flush=True)


if __name__=='__main__':
    main()
