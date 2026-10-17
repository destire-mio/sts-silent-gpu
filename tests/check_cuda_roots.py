"""Replay E2 prefixes on both batch implementations and compare every output byte."""
import argparse
import gzip
import json
from pathlib import Path

import torch
import spire
import spire_gpu
from check_cuda_python import FIELDS


def check(path):
    cpu = spire.TrainingBatch(1, 1, 3000, [1])
    gpu = spire_gpu.TrainingBatch(1, 3000, [1])
    arrays = {name: torch.as_tensor(getattr(gpu, name), device='cuda') for name in FIELDS}
    count = 0
    with gzip.open(path, 'rt') as stream:
        for text in stream:
            root = json.loads(text)['root']
            cpu.reset_to(0, root['seed'], root['candidates'])
            gpu.reset_to(0, root['seed'], root['candidates'])
            assert not cpu.faults.any(), root['seed']
            for name, value in arrays.items():
                assert value.cpu().numpy().tobytes() == getattr(cpu, name).tobytes(), (root['seed'], name)
            count += 1
    assert count > 0
    print(f'CUDA E2 roots: {count} prefixes, all observation/status bytes match')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('roots', type=Path)
    check(parser.parse_args().roots)
