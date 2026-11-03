"""Edit the 64h CLS policy's score features or action-kind embeddings without training."""
import argparse
from dataclasses import asdict
from hashlib import sha256
import json
import math
from pathlib import Path

import torch

from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig


ACTION_KINDS = ('play', 'end_turn', 'choose', 'potion_use', 'potion_discard', 'proceed', 'skip')
BLOCKS = dict(subject=0, target=1, option=2, context=4)
MODEL_CONFIG = ModelConfig(width=384, layers=6, heads=6, feedforward=1536)


def tensor_hash(tensor):
    return sha256(tensor.detach().cpu().contiguous().numpy().tobytes()).hexdigest()


@torch.no_grad()
def edit_kind_row(state, action_kind, strength):
    assert math.isfinite(strength) and strength != 0
    row = ACTION_KINDS.index(action_kind) + 1
    width = state['kind.weight'].shape[1]
    direction = state['score.2.weight'][0] @ state['score.0.weight'][:, 3 * width:4 * width]
    norm = float(direction.norm())
    assert math.isfinite(norm) and norm > 0
    edited = {name: tensor.detach().clone() for name, tensor in state.items()}
    edited['kind.weight'][row] += strength * direction / norm
    assert all(torch.isfinite(tensor).all() for tensor in edited.values())
    changes = {name: int((tensor != edited[name]).sum()) for name, tensor in state.items()}
    assert 0 < changes['kind.weight'] <= width
    assert all(count == 0 for name, count in changes.items() if name != 'kind.weight')
    tensor_evidence = {name: dict(shape=list(tensor.shape), dtype=str(tensor.dtype),
        before_sha256=tensor_hash(tensor), after_sha256=tensor_hash(edited[name]),
        changed_scalars=changes[name]) for name, tensor in state.items()}
    return edited, dict(action_kind=action_kind, kind_row=row, strength=strength,
        direction='score.2.weight[0] @ score.0.weight[:, 3*width:4*width], L2 normalized',
        direction_norm=norm, direction_sha256=tensor_hash(direction / norm),
        changed_scalars=sum(changes.values()), total_scalars=sum(t.numel() for t in state.values()),
        changed_tensors=[name for name, count in changes.items() if count],
        delta_l2=float((edited['kind.weight'] - state['kind.weight']).norm()),
        tensor_evidence=tensor_evidence,
        interpretation='Linear weight-path intervention; GELU makes logit changes state dependent. '
                       'No monotonicity or gameplay improvement is assumed.')


@torch.no_grad()
def edit_block(state, block, scale):
    assert math.isfinite(scale) and scale >= 0 and scale != 1
    width = state['kind.weight'].shape[1]
    start = BLOCKS[block] * width
    edited = {name: tensor.detach().clone() for name, tensor in state.items()}
    edited['score.0.weight'][:, start:start + width] *= scale
    assert all(torch.isfinite(tensor).all() for tensor in edited.values())
    changes = {name: int((tensor != edited[name]).sum()) for name, tensor in state.items()}
    assert 0 < changes['score.0.weight'] <= width * width
    assert all(count == 0 for name, count in changes.items() if name != 'score.0.weight')
    return edited, dict(block=block, scale=scale, columns=[start, start + width],
        changed_scalars=sum(changes.values()), total_scalars=sum(t.numel() for t in state.values()),
        changed_tensors=[name for name, count in changes.items() if count],
        delta_l2=float((edited['score.0.weight'] - state['score.0.weight']).norm()),
        tensor_evidence={name: dict(shape=list(tensor.shape), dtype=str(tensor.dtype),
            before_sha256=tensor_hash(tensor), after_sha256=tensor_hash(edited[name]),
            changed_scalars=changes[name]) for name, tensor in state.items()},
        interpretation='Scale one existing head feature block; encoded entities and value head remain unchanged. '
                       'No gameplay improvement is assumed.')


def write_edit(checkpoint, output, manifest, *, expected_sha256, block=None, scale=None, action_kind=None, strength=None):
    assert ((block is not None and scale is not None and action_kind is None and strength is None)
            or (action_kind is not None and strength is not None and block is None and scale is None))
    checkpoint, output, manifest = (Path(path).resolve() for path in (checkpoint, output, manifest))
    assert len({checkpoint, output, manifest}) == 3
    assert not output.exists() and not manifest.exists()
    parent_hash = sha256(checkpoint.read_bytes()).hexdigest()
    assert parent_hash == expected_sha256, 'Checkpoint SHA256 differs from the requested parent model'
    source = torch.load(checkpoint, map_location='cpu', weights_only=True)
    assert isinstance(source, dict) and all(isinstance(value, torch.Tensor) for value in source.values()), \
        'Expected a raw 64h CLS state_dict, not a wrapped checkpoint'
    policy = CLSPolicy(MODEL_CONFIG)
    policy.load_state_dict(source, strict=True)
    assert sum(value.numel() for value in source.values()) == 12435074
    edited, evidence = (edit_block(source, block, scale) if block is not None
                        else edit_kind_row(source, action_kind, strength))
    policy.load_state_dict(edited, strict=True)
    with output.open('xb') as stream:
        torch.save(edited, stream)
    reloaded = torch.load(output, map_location='cpu', weights_only=True)
    policy.load_state_dict(reloaded, strict=True)
    assert all(torch.equal(tensor, reloaded[name]) for name, tensor in edited.items())
    assert sha256(checkpoint.read_bytes()).hexdigest() == parent_hash
    result = dict(format_version=1, evaluation_only=True, raw_state_dict=True, checkpoint=str(checkpoint),
        parent_checkpoint_sha256=parent_hash, parent_file_unchanged=True,
        output=str(output), output_sha256=sha256(output.read_bytes()).hexdigest(),
        model_class='train.e4_stage12.CLSPolicy', source_model_config=asdict(MODEL_CONFIG),
        precision='Model-only weights do not encode inference precision; keep evaluation precision fixed across both arms.',
        strict_reload_verified=True, gradients_computed=0, optimizer_steps=0, edit=evidence)
    with manifest.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', type=Path, required=True)
    parser.add_argument('--expected-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument('--block', choices=BLOCKS)
    selection.add_argument('--action-kind', choices=ACTION_KINDS)
    parser.add_argument('--scale', type=float)
    parser.add_argument('--strength', type=float)
    args = parser.parse_args()
    if args.block is not None and (args.scale is None or args.strength is not None):
        parser.error('--block requires --scale and excludes --strength')
    if args.action_kind is not None and (args.strength is None or args.scale is not None):
        parser.error('--action-kind requires --strength and excludes --scale')
    torch.set_num_threads(1)
    result = write_edit(args.checkpoint, args.output, args.manifest, expected_sha256=args.expected_sha256,
                        block=args.block, scale=args.scale,
                        action_kind=args.action_kind, strength=args.strength)
    print(json.dumps(dict(output=result['output'], output_sha256=result['output_sha256'],
                          changed_scalars=result['edit']['changed_scalars'], evaluation_only=True)))


if __name__ == '__main__':
    main()
