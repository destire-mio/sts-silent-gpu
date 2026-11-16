"""Training critic observations stored separately from policy inputs."""

import torch
import torch.distributed as dist


def critic_observation(public, packed_tokens, draw_slots):
    columns = packed_tokens.ne(0).any(dim=0).nonzero().flatten()
    if not len(columns):
        return public
    tokens = packed_tokens.index_select(1, columns).long()
    draw = columns < draw_slots
    position = torch.where(draw, columns + 1, columns - draw_slots + 1)
    present = tokens != 0

    tags = public["tags"].new_zeros((len(tokens), 4, len(columns)))
    tags[:, 0] = torch.where(draw, 3, 4) * present
    tags[:, 1] = 3 * (draw & present)
    ids = public["ids"].new_zeros((len(tokens), 4, len(columns)))
    ids[:, 0] = tokens & 511
    ids[:, 2] = position * present
    ids[:, 3] = (tokens >> 9) * ~draw
    values = public["values"].new_zeros((len(tokens), 6, len(columns)))
    values[:, 0] = (tokens >> 9) * draw
    return {**public, "tags": torch.cat((public["tags"], tags), dim=2),
            "ids": torch.cat((public["ids"], ids), dim=2),
            "values": torch.cat((public["values"], values), dim=2),
            "entity_mask": torch.cat((public["entity_mask"], present), dim=1)}


def private_value_diagnostics(fragment):
    public = fragment["public_rollout_values"]
    valid = fragment["action_valid"] & public.isfinite()
    result = {}
    for name, combat in (("combat", True), ("noncombat", False)):
        mask = valid & (fragment["combat"] == combat)
        target = fragment["returns"][mask].double()
        private_error = target - fragment["values"][mask].double()
        public_error = target - public[mask].double()
        moments = torch.stack((mask.sum(), target.sum(), target.square().sum(),
                               private_error.sum(), private_error.square().sum(),
                               public_error.sum(), public_error.square().sum()))
        dist.all_reduce(moments)
        n, total, squares, private, private_sq, ordinary, ordinary_sq = moments.tolist()
        variance = squares / n - (total / n)**2 if n else 0.
        result[name] = dict(samples=int(n),
                            critic_explained_variance=1. - (private_sq / n - (private / n)**2) / variance if variance > 0 else None,
                            public_v_explained_variance=1. - (ordinary_sq / n - (ordinary / n)**2) / variance if variance > 0 else None)
    return result
