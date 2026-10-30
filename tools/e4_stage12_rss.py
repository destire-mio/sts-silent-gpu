"""Measure one 2048-lane rank for two collection rounds before sizing Stage12."""
import argparse
import gc
import json
from pathlib import Path
import resource

import torch

from train.e4_stage12 import CLSPolicy, NoSLSearch, NoSLCollector, save_rank


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--checkpoint', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    torch.set_num_threads(1)
    torch.cuda.set_device(0)
    state = torch.load(args.checkpoint, map_location='cpu', weights_only=False)
    policy = CLSPolicy().to('cuda').eval()
    policy.load_state_dict(state['model'])
    policy.encoder_fp16 = state['state']['precision'] == 'encoder_fp16_heads_fp32'
    search = NoSLSearch(2048, torch.device('cuda', 0), 20261031)
    collector = NoSLCollector(search, torch.Generator().manual_seed(20261131))
    rows = []
    for iteration in range(2):
        fragment = collector.collect(policy, length=64)
        torch.cuda.synchronize()
        rows.append(dict(round=iteration+1, decisions=collector.decisions,
                         rss_peak_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*1024))
        if iteration == 1:
            save_rank(args.output.with_suffix('.rank.pt'), collector, dict(fragment=fragment),
                      state['state']['update'], torch.Generator(device='cuda').manual_seed(1),
                      dict(ready=None, seconds=0., decisions_start=0))
        del fragment
        collector.completed.clear()
        gc.collect()
    result = dict(lanes=2048, rounds=2, checkpoint_update=state['state']['update'],
                  rss_peak_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*1024, measurements=rows,
                  checkpoint_modified=False, optimizer_updates=0, includes_checkpoint_memory_peak=True)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result), flush=True)


if __name__ == '__main__':
    main()
