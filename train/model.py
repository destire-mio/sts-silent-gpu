from dataclasses import dataclass

import torch
from torch import nn
from torch.distributions import Categorical


FIELDS = ("tags", "ids", "values", "candidate_kinds", "candidate_refs", "entity_mask", "candidate_mask")
SCREENS = ("combat", "hand_select", "grid", "victory", "death", "none", "card_reward",
           "combat_reward", "event", "map", "room", "shop", "boss_reward")


@dataclass
class ModelConfig:
    width: int = 128
    layers: int = 4
    heads: int = 4
    feedforward: int = 256


def views(batch, device=torch.device("cpu")):
    return {name: torch.as_tensor(getattr(batch, name), device=device) for name in FIELDS}


def network_inputs(arrays, device, slots):
    arrays = {name: torch.as_tensor(array, device=device) for name, array in arrays.items()}
    masks = {}
    for name in FIELDS[5:]:
        packed = arrays[name][slots]
        masks[name] = ((packed[..., None].long() >> torch.arange(8, device=device)) & 1).flatten(1).bool()
    ne = int(masks["entity_mask"].sum(-1).max())
    nc = max(1, int(masks["candidate_mask"].sum(-1).max()))
    result = {name: arrays[name][slots][..., :nc if name.startswith("candidate") else ne].to(
        torch.float32 if name == "values" else torch.long) for name in FIELDS[:5]}
    result.update(entity_mask=masks["entity_mask"][:, :ne], candidate_mask=masks["candidate_mask"][:, :nc])
    return result


def record_lengths(data, offsets, indices):
    data = torch.as_tensor(data)
    offsets = torch.as_tensor(offsets, device=data.device, dtype=torch.int64)
    starts = offsets[indices]
    return (data[starts].long() + 256 * data[starts + 1].long(),
            data[starts + 2].long() + 256 * data[starts + 3].long())


def packed_inputs(data, offsets, indices, device):
    data = torch.as_tensor(data, device=device)
    offsets = torch.as_tensor(offsets, device=device, dtype=torch.int64)
    entities, candidates = record_lengths(data, offsets, indices)
    ne, nc = int(entities.max()), max(1, int(candidates.max()))
    cursor = offsets[indices] + 4
    result = {}
    for name, rows, lengths, width, word in (("tags", 4, entities, ne, 1), ("ids", 4, entities, ne, 2),
            ("values", 6, entities, ne, 2), ("candidate_kinds", 1, candidates, nc, 1),
            ("candidate_refs", 3, candidates, nc, 2)):
        col = torch.arange(width, device=device)
        positions = cursor[:, None, None] + word * (torch.arange(rows, device=device)[None, :, None] *
                                                    lengths[:, None, None] + col)
        valid = col[None, None, :] < lengths[:, None, None]
        positions = torch.where(valid, positions, 0)
        value = data[positions].long()
        if word == 2:
            value = value + 256 * data[positions + 1].long()
            value = torch.where(value >= 32768, value - 65536, value)
        value = torch.where(valid, value, 0)
        result[name] = value[:, 0] if rows == 1 else value
        cursor = cursor + rows * word * lengths
    result["values"] = result["values"].float()
    result["entity_mask"] = torch.arange(ne, device=device) < entities[:, None]
    result["candidate_mask"] = torch.arange(nc, device=device) < candidates[:, None]
    return result


def store_inputs(batch, arrays, slots, data, offsets):
    e = arrays["entity_counts"][slots].long()
    c = arrays["candidate_counts"][slots].long()
    offsets[1:] = offsets[0] + (4 + 24 * e + 7 * c).cumsum(0)
    assert int(offsets[-1]) <= len(data), "Compact rollout storage exhausted; increase storage_mib"
    batch.store_tensors(slots, data, offsets)


def length_batches(indices, entities, candidates, size, rng):
    shuffled = indices[torch.randperm(len(indices), generator=rng, device=indices.device)]
    key = (entities[shuffled] // 16) * 129 + candidates[shuffled]
    ordered = shuffled[torch.argsort(key, stable=True)]
    batches = list(ordered.split(size))
    return [batches[i] for i in torch.randperm(len(batches), generator=rng, device=indices.device).tolist()]


class Policy(nn.Module):
    def __init__(self, config):
        super().__init__()
        self.config = config
        width = config.width
        self.tags = nn.ModuleList(nn.Embedding(n, width, padding_idx=0) for n in (13, 12, 256, 256))
        self.ids = nn.ModuleList(nn.Embedding(n, width, padding_idx=0) for n in (512, 385, 385, 512))
        self.numbers = nn.Linear(6, width)
        self.input_norm = nn.LayerNorm(width)
        self.encoder = nn.ModuleList(nn.TransformerEncoderLayer(
            width, config.heads, config.feedforward, dropout=0, batch_first=True, activation="gelu")
            for _ in range(config.layers))
        self.kind = nn.Embedding(8, width, padding_idx=0)
        self.score = nn.Sequential(nn.Linear(5 * width, width), nn.GELU(), nn.Linear(width, 1))
        self.value = nn.Sequential(nn.Linear(width, width), nn.GELU(), nn.Linear(width, 1), nn.Sigmoid())

    def forward(self, observation):
        mask = observation["entity_mask"]
        values = observation["values"].transpose(1, 2)
        entities = self.numbers(values.sign() * values.abs().log1p())
        for row, embedding in enumerate(self.tags):
            entities = entities + embedding(observation["tags"][:, row])
        for row, embedding in enumerate(self.ids):
            entities = entities + embedding(observation["ids"][:, row])
        entities = self.input_norm(entities)
        for layer in self.encoder:
            entities = layer(entities, src_key_padding_mask=~mask)
        pooled = (entities * mask.unsqueeze(-1)).sum(1) / mask.sum(1, keepdim=True)
        table = torch.cat((torch.zeros_like(entities[:, :1]), entities), dim=1)
        refs = observation["candidate_refs"].transpose(1, 2)
        rows = torch.arange(len(table), device=table.device)[:, None, None]
        pointed = table[rows, refs].flatten(2)
        kinds = self.kind(observation["candidate_kinds"])
        context = pooled[:, None].expand(-1, kinds.shape[1], -1)
        logits = self.score(torch.cat((pointed, kinds, context), dim=-1)).squeeze(-1)
        logits = logits.masked_fill(~observation["candidate_mask"], -torch.inf)
        return logits, self.value(pooled).squeeze(-1)


def distribution(logits, legal):
    policy = Categorical(logits=logits, validate_args=False)
    count = legal.sum(-1)
    entropy = policy.entropy() / count.clamp_min(2).float().log()
    return policy, entropy
