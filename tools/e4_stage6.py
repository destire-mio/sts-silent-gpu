import argparse
from datetime import timedelta
import gzip
import json
import os
from pathlib import Path
import subprocess

import torch
import torch.distributed as dist

from train.checkpoint import engine_commit
from tools.e4_run import atomic_json
from train.e4_stage6.phase6_allocation import AllocationClock
from train.e4_stage6.phase6_runtime import Phase6Runtime
from train.e4_stage6.phase6_storage import file_digest


def read_json_gzip(path):
    with gzip.open(path,'rt',encoding='utf-8') as stream:
        return json.load(stream)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--context',type=Path,required=True)
    args=parser.parse_args()
    context=json.loads(args.context.read_bytes())
    job_id=os.environ['SLURM_JOB_ID']
    rank,world=int(os.environ['RANK']),int(os.environ['WORLD_SIZE'])
    local_rank=int(os.environ['LOCAL_RANK'])
    assert world in (7,8) and rank==local_rank and int(os.environ['SLURM_JOB_NUM_NODES'])==1
    commit=engine_commit()
    repository=Path(__file__).resolve().parents[1]
    assert not subprocess.check_output(['git','status','--porcelain'],cwd=repository).strip()
    clock=AllocationClock(context,job_id,world,commit)
    torch.set_num_threads(1)
    device=torch.device('cuda',local_rank)
    torch.cuda.set_device(device)
    dist.init_process_group('nccl',timeout=timedelta(minutes=70))
    assert dist.get_rank()==rank and dist.get_world_size()==world
    inputs=context['inputs']
    paths=[record['path'] for record in inputs]
    assert len(paths)==len(set(paths))
    expected={record['path']:record['sha256'] for record in inputs}
    if rank==0:
        assert all(file_digest(Path(path))==digest for path,digest in expected.items())
    dist.barrier()
    sources=context['sources']
    assert len(sources)==8 and [s['rank'] for s in sources]==list(range(8))
    source_paths=[s['path'] for s in sources]
    source_hashes=[s['sha256'] for s in sources]
    anchors=context['evaluation_anchors']
    assert len(anchors)==world and [s['rank'] for s in anchors]==list(range(world))
    for record in sources+anchors+[context['exam'],context['baseline']]:
        assert expected[record['path']]==record['sha256']
    exam=read_json_gzip(context['exam']['path'])
    baseline=read_json_gzip(context['baseline']['path'])
    anchor=torch.load(anchors[rank]['path'],map_location='cpu',weights_only=False)
    assert set(anchor)=={'seed_rng','search_rng'}
    binding=dict(world=world,runtime_commit=commit,run_id=context['run_id'],source_hashes=source_hashes,
        exam_sha256=context['exam']['sha256'],baseline_sha256=context['baseline']['sha256'],
        evaluation_anchor_hashes=[s['sha256'] for s in anchors])
    runtime=Phase6Runtime(context['output'],binding,source_paths,source_hashes,exam,anchor,baseline,
                         device,Path(context['stop_file']),job_id)
    result=runtime.run(clock,examination_reserve_remaining=3600.,overhead_reserve=210.*world/8)
    if rank==0:
        directory=Path(context['output'])/'allocations'/str(job_id)
        directory.mkdir(parents=True,exist_ok=True)
        atomic_json(directory/'trainer-result.json',dict(result,job_id=job_id,runtime_commit=commit,
            phase_remaining_node_seconds_observed=clock.remaining_node_seconds(),
            accounting_scope='Guard uses elapsed allocation time including startup; parent finalSlurm GPU seconds /8 settles cost once.'))
    dist.barrier()
    dist.destroy_process_group()


if __name__=='__main__':
    main()
