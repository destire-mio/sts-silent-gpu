"""Evaluate frozen raw CLS weights with the established Stage12 full-run policy."""
import argparse
from collections import Counter
from hashlib import sha256
import json
from pathlib import Path
import statistics
import subprocess
import time

import torch
from torch import nn

from train import OBSERVATION_VERSION
from train.e4 import AllocationEnded
from train.e4_evaluation_ledger import EvaluationLedgerCollector
from train.e4_stage12 import CLSPolicy, NoSLSearch
from train.model import ModelConfig


MODEL_CONFIG = dict(width=384, layers=6, heads=6, feedforward=1536)
PARAMETER_COUNT = 12435074
ROOT = Path(__file__).resolve().parents[1]
SOURCES = ('train/model.py', 'train/e4.py', 'train/e4_rewards.py', 'train/e4_stage12.py',
           'train/e4_evaluation_ledger.py', 'tools/e4_weight_edit_eval.py')


def digest(path):
    return sha256(Path(path).read_bytes()).hexdigest()


def read_state_dict(checkpoint, expected_sha256):
    assert digest(checkpoint) == expected_sha256, 'Checkpoint SHA256 differs from the requested model'
    state = torch.load(checkpoint, map_location='cpu', weights_only=True)
    assert isinstance(state, dict) and 'cls' in state, 'Expected a raw CLS state_dict'
    assert all(isinstance(value, torch.Tensor) for value in state.values()), 'Expected only model tensors'
    assert all(torch.isfinite(value).all() for value in state.values()), 'Nonfinite model weights'
    return state


def load_policy(checkpoint, expected_sha256):
    state = read_state_dict(checkpoint, expected_sha256)
    policy = CLSPolicy(ModelConfig(**MODEL_CONFIG)).eval()
    policy.load_state_dict(state, strict=True)
    assert sum(value.numel() for value in policy.parameters()) == PARAMETER_COUNT
    policy.encoder_fp16 = True
    return policy


class CPUEnvironmentPolicy(nn.Module):
    def __init__(self, policy):
        super().__init__()
        self.policy = policy

    def forward(self, observation):
        logits, values = self.policy({name: value.to('cuda') for name, value in observation.items()})
        logits, values = logits.cpu(), values.cpu()
        assert torch.isfinite(logits[observation['candidate_mask']]).all() and torch.isfinite(values).all()
        return logits, values


def evaluation_receipt(episodes, seeds, complete):
    rows = sorted(episodes, key=lambda row: row['seed'])
    actual = [row['seed'] for row in rows]
    assert len(set(seeds)) == len(seeds) and len(set(actual)) == len(actual)
    assert set(actual) <= set(seeds)
    assert not complete or set(actual) == set(seeds), 'Incomplete evaluation cannot be reported complete'
    assert all(row['outcome'] in ('death', 'victory', 'act3_win', 'stalled') for row in rows)
    count = len(rows)
    return dict(complete=complete, requested_seeds=seeds,
        unfinished_seeds=[seed for seed in seeds if seed not in set(actual)], episodes=rows,
        summary=dict(episodes=count, mean_floor=statistics.mean(row['floor'] for row in rows) if count else None,
            outcomes=dict(Counter(row['outcome'] for row in rows)),
            unresolved_stalls=sum(row['outcome'] == 'stalled' for row in rows),
            stall_classes=dict(Counter(row['stall_class'] for row in rows if row['outcome'] == 'stalled')),
            boss_counts={key: sum(row['reach'][key] for row in rows)
                         for key in ('act1_boss', 'act2_boss', 'act3_first_boss')},
            act3_wins=sum(row['reach']['act3_second_boss'] for row in rows),
            heart_wins=sum(row['reach']['heart'] for row in rows),
            act4_entries=sum(row['reach']['act4'] for row in rows),
            shield_spear_wins=sum(row['reach']['shield_spear'] for row in rows),
            key_counts={key: sum(row['keys'][key] for row in rows) for key in ('ruby', 'emerald', 'sapphire')}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', type=Path, required=True)
    parser.add_argument('--expected-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--first-seed', type=int, required=True)
    parser.add_argument('--episodes', type=int, default=128)
    parser.add_argument('--lanes', type=int, default=128)
    parser.add_argument('--seconds', type=float, default=900.)
    args = parser.parse_args()
    assert not args.output.exists()
    assert args.episodes > 0 and 0 < args.lanes <= 128 and args.seconds > 0
    assert OBSERVATION_VERSION == 13 and torch.cuda.is_available()
    import spire

    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    torch.backends.cuda.matmul.allow_tf32 = False
    torch.backends.cudnn.allow_tf32 = False
    policy = load_policy(args.checkpoint, args.expected_sha256).to('cuda')
    search = NoSLSearch(args.lanes, torch.device('cpu'), 101, greedy=True)
    collector = EvaluationLedgerCollector(search, torch.Generator().manual_seed(101), rank=0)
    seeds = list(range(args.first_seed, args.first_seed + args.episodes))
    source_hashes = {name: digest(ROOT / name) for name in SOURCES}
    torch.cuda.reset_peak_memory_stats()
    started = time.perf_counter()
    search.deadline = time.monotonic() + args.seconds
    complete = True
    try:
        collector.evaluate(CPUEnvironmentPolicy(policy).eval(), seeds)
    except AllocationEnded:
        complete = False
    elapsed = time.perf_counter() - started
    result = evaluation_receipt(collector.completed, seeds, complete)
    result.update(checkpoint=str(args.checkpoint.resolve()), checkpoint_sha256=args.expected_sha256,
        model_config=MODEL_CONFIG, parameter_count=PARAMETER_COUNT, observation_version=OBSERVATION_VERSION,
        selection='argmax', precision='encoder_fp16_heads_fp32', lanes=args.lanes,
        masks='Unmodified NoSLSearch: engine-legal combat; established noncombat reward, step0, map-return and repeat-probe masks',
        stall_limits=dict(same_floor_steps=3000, noncombat_floor_steps=200, comparison='strictly greater than'),
        outcome_rule='EvaluationLedgerCollector labels Act3 second-boss reach without Heart as act3_win; this does not establish post-Act3 survival',
        elapsed_seconds=elapsed, decisions=collector.decisions, decisions_per_second=collector.decisions / elapsed,
        peak_gpu_bytes=torch.cuda.max_memory_allocated(), loop_metrics=search.loop_metrics,
        fault_counts=dict(search.fault_counts), source_sha256=source_hashes,
        code_commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        environment=dict(torch_version=str(torch.__version__), cuda_version=torch.version.cuda,
            gpu=torch.cuda.get_device_name(), native_engine_path=spire.__file__, native_engine_sha256=digest(spire.__file__)))
    assert digest(args.checkpoint) == args.expected_sha256
    assert all(digest(ROOT / name) == identity for name, identity in source_hashes.items())
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
    print(json.dumps({key: result[key] for key in ('complete', 'summary', 'elapsed_seconds', 'decisions_per_second')}))


if __name__ == '__main__':
    main()
