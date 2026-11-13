"""Explicit evaluation entry point; never invoked by the training chain."""
import argparse
import json
from pathlib import Path
import torch
from train.e4_lookahead import LookaheadSearch
from train.e4_stage12 import CLSPolicy, NoSLCollector
from train.model import ModelConfig


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--depth', choices=('turn', 'combat'), default='turn')
    parser.add_argument('--override-se', type=float, default=1.0,
                        help='Paired SE multiplier. A non-default multiplier or validation requires two nonzero differences.')
    parser.add_argument('--validate', type=int, default=0, help='Independent paired draws to confirm each proposed override.')
    parser.add_argument('--device', default='cpu')
    parser.add_argument('--seed-start', type=int, default=2000000000)
    parser.add_argument('--episodes', type=int, default=1024)
    parser.add_argument('--lanes', type=int, default=128)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    torch.set_num_threads(1)
    device = torch.device(args.device)
    policy = CLSPolicy(ModelConfig(384, 6, 6, 1536)).to(device).eval()
    policy.load_state_dict(torch.load(args.model, map_location=device, weights_only=True), strict=True)
    policy.encoder_fp16 = device.type == 'cuda'
    search = LookaheadSearch(args.lanes, device, 130, args.depth, override_se=args.override_se, validate=args.validate)
    collector = NoSLCollector(search, torch.Generator().manual_seed(130))
    episodes = collector.evaluate(policy, list(range(args.seed_start, args.seed_start + args.episodes)))
    args.output.write_text(json.dumps(dict(depth=args.depth, episodes=episodes,
                                          preview_steps=search.preview_steps)) + '\n')


if __name__ == '__main__':
    main()
