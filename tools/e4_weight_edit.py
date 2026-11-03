"""Edit one action-kind embedding through its linear score path, without training."""
import argparse
from hashlib import sha256
import json
import math
from pathlib import Path

import torch

from train.model import ModelConfig, Policy


ACTION_KINDS = ('play', 'end_turn', 'choose', 'potion_use', 'potion_discard', 'proceed', 'skip')


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


def write_edit(checkpoint, output, manifest, action_kind, strength):
    checkpoint, output, manifest = (Path(path).resolve() for path in (checkpoint, output, manifest))
    assert len({checkpoint, output, manifest}) == 3
    assert not output.exists() and not manifest.exists()
    parent_hash = sha256(checkpoint.read_bytes()).hexdigest()
    source = torch.load(checkpoint, map_location='cpu', weights_only=False)
    policy = Policy(ModelConfig(**source['config']['model']))
    policy.load_state_dict(source['model'], strict=True)
    edited, evidence = edit_kind_row(source['model'], action_kind, strength)
    policy.load_state_dict(edited, strict=True)
    artifact = dict(format_version=1, artifact_type='weight_edit_evaluation', evaluation_only=True,
        model=edited, config=source['config'], engine_commit=source['engine_commit'],
        parent_checkpoint_sha256=parent_hash, weight_edit=evidence)
    with output.open('xb') as stream:
        torch.save(artifact, stream)
    reloaded = torch.load(output, map_location='cpu', weights_only=False)
    policy.load_state_dict(reloaded['model'], strict=True)
    assert all(torch.equal(tensor, reloaded['model'][name]) for name, tensor in edited.items())
    assert sha256(checkpoint.read_bytes()).hexdigest() == parent_hash
    result = dict(format_version=1, evaluation_only=True, checkpoint=str(checkpoint),
        parent_checkpoint_sha256=parent_hash, parent_file_unchanged=True,
        output=str(output), output_sha256=sha256(output.read_bytes()).hexdigest(),
        source_engine_commit=source['engine_commit'], source_model_config=source['config']['model'],
        strict_reload_verified=True, gradients_computed=0, optimizer_steps=0, edit=evidence)
    with manifest.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(result, indent=2) + '\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--action-kind', choices=ACTION_KINDS, required=True)
    parser.add_argument('--strength', type=float, required=True)
    args = parser.parse_args()
    torch.set_num_threads(1)
    result = write_edit(args.checkpoint, args.output, args.manifest, args.action_kind, args.strength)
    print(json.dumps(dict(output=result['output'], output_sha256=result['output_sha256'],
                          changed_scalars=result['edit']['changed_scalars'], evaluation_only=True)))


if __name__ == '__main__':
    main()
