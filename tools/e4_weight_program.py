"""Write fixed action-kind preferences into selected scoring units without training."""
import argparse
from hashlib import sha256
import json
from pathlib import Path

import torch
from torch.nn import functional as F

from tools.e4_weight_edit import ACTION_KINDS, tensor_hash
from train.model import ModelConfig, Policy


@torch.no_grad()
def program_head(state, context_scale, priors):
    edited = {name: tensor.detach().clone() for name, tensor in state.items()}
    width = state['kind.weight'].shape[1]
    assert len({unit for unit, _, _ in priors}) == len(priors)
    edited['score.0.weight'][:, 4 * width:5 * width] *= context_scale
    for unit, action_kind, strength in priors:
        assert 0 <= unit < width
        row = ACTION_KINDS.index(action_kind) + 1
        targets = torch.full((7,), -6., dtype=torch.float64)
        targets[row - 1] = 4.
        detector = torch.linalg.lstsq(state['kind.weight'][1:].double(), targets, driver='gelsd').solution.float()
        assert torch.max(torch.abs(state['kind.weight'][1:] @ detector - targets.float())) < 1e-4
        edited['score.0.weight'][unit].zero_()
        edited['score.0.weight'][unit, 3 * width:4 * width] = detector
        edited['score.0.bias'][unit] = 0
        edited['score.2.weight'][0, unit] = strength / F.gelu(torch.tensor(4.)).item()
    assert all(torch.isfinite(tensor).all() for tensor in edited.values())
    return edited


def write_program(checkpoint, output, manifest, context_scale, priors):
    checkpoint, output, manifest = [Path(p).resolve() for p in (checkpoint, output, manifest)]
    assert len({checkpoint, output, manifest}) == 3 and not output.exists() and not manifest.exists()
    parent_hash = sha256(checkpoint.read_bytes()).hexdigest()
    source = torch.load(checkpoint, map_location='cpu', weights_only=True)
    policy = Policy(ModelConfig(**source['config']['model']))
    policy.load_state_dict(source['model'], strict=True)
    edited = program_head(source['model'], context_scale, priors)
    policy.load_state_dict(edited, strict=True)
    evidence = dict(context_scale=context_scale, priors=priors,
        interpretation='Each selected scoring unit is replaced by a fixed kind detector. '
                       'Its original contribution is removed; this does not add contextual understanding.',
        parent_sha256=parent_hash, gradients=0, optimizer_steps=0,
        changed_scalars=sum(int((v != edited[k]).sum()) for k, v in source['model'].items()),
        total_scalars=sum(v.numel() for v in edited.values()),
        tensors={k: dict(before_sha256=tensor_hash(v), after_sha256=tensor_hash(edited[k]),
                        changed_scalars=int((v != edited[k]).sum())) for k, v in source['model'].items()})
    with output.open('xb') as stream:
        torch.save(dict(model=edited, config=source['config'], engine_commit=source['engine_commit'],
                        evaluation_only=True, weight_program=evidence), stream)
    reloaded = torch.load(output, map_location='cpu', weights_only=True)
    policy.load_state_dict(reloaded['model'], strict=True)
    assert all(torch.equal(v, reloaded['model'][k]) for k, v in edited.items())
    assert sha256(checkpoint.read_bytes()).hexdigest() == parent_hash
    evidence.update(output=str(output), output_sha256=sha256(output.read_bytes()).hexdigest(),
                    source_unchanged=True, strict_reload_verified=True)
    with manifest.open('x', encoding='utf-8') as stream:
        json.dump(evidence, stream, indent=2)
    return evidence


def parse_prior(text):
    unit, action_kind, strength = text.split(':')
    assert action_kind in ACTION_KINDS
    return int(unit), action_kind, float(strength)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkpoint', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--context-scale', type=float, required=True)
    parser.add_argument('--prior', type=parse_prior, action='append', default=[])
    args = parser.parse_args()
    torch.set_num_threads(1)
    result = write_program(args.checkpoint, args.output, args.manifest, args.context_scale, args.prior)
    print(json.dumps({key: result[key] for key in ('output', 'output_sha256', 'changed_scalars')}))


if __name__ == '__main__':
    main()
