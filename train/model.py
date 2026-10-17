from dataclasses import dataclass

import numpy as np
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


def views(batch):
    return {name: getattr(batch, name) for name in FIELDS}


def network_inputs(arrays, device, slots):
    entity_mask = np.unpackbits(arrays["entity_mask"][slots], axis=-1, bitorder="little").astype(bool)
    candidate_mask = np.unpackbits(arrays["candidate_mask"][slots], axis=-1, bitorder="little").astype(bool)
    entities = int(entity_mask.sum(axis=1).max())
    candidates = max(1, int(candidate_mask.sum(axis=1).max()))
    result = {}
    for name in FIELDS[:5]:
        width = candidates if name.startswith("candidate") else entities
        dtype = torch.float32 if name == "values" else torch.long
        result[name] = torch.tensor(arrays[name][..., :width][slots], device=device, dtype=dtype)
    result["entity_mask"] = torch.tensor(entity_mask[:, :entities], device=device)
    result["candidate_mask"] = torch.tensor(candidate_mask[:, :candidates], device=device)
    return result


def record_lengths(data, offsets, indices):
    starts = offsets[indices].astype(np.int64)
    return (data[starts].astype(np.int32) + 256 * data[starts + 1].astype(np.int32),
            data[starts + 2].astype(np.int32) + 256 * data[starts + 3].astype(np.int32))


def packed_inputs(data, offsets, indices, device):
    entities, candidates = record_lengths(data, offsets, indices)
    count, ne, nc = len(indices), int(entities.max()), max(1, int(candidates.max()))
    arrays = dict(tags=np.zeros((count, 4, ne), dtype=np.uint8),
                  ids=np.zeros((count, 4, ne), dtype=np.int16),
                  values=np.zeros((count, 6, ne), dtype=np.int16),
                  candidate_kinds=np.zeros((count, nc), dtype=np.uint8),
                  candidate_refs=np.zeros((count, 3, nc), dtype=np.int16))
    for row, (index, e, c) in enumerate(zip(indices, entities, candidates)):
        cursor = int(offsets[index]) + 4
        for name, rows, width, dtype in (("tags", 4, e, "u1"), ("ids", 4, e, "<i2"),
                                        ("values", 6, e, "<i2"), ("candidate_kinds", 1, c, "u1"),
                                        ("candidate_refs", 3, c, "<i2")):
            length = int(rows * width)
            source = np.frombuffer(data, dtype=dtype, count=length, offset=cursor)
            arrays[name][row, ..., :width] = source.reshape(arrays[name][row, ..., :width].shape)
            cursor += length * np.dtype(dtype).itemsize
    result = {name: torch.tensor(array, device=device, dtype=torch.float32 if name == "values" else torch.long)
              for name, array in arrays.items()}
    result["entity_mask"] = torch.tensor(np.arange(ne) < entities[:, None], device=device)
    result["candidate_mask"] = torch.tensor(np.arange(nc) < candidates[:, None], device=device)
    return result


def length_batches(indices, entities, candidates, size, rng):
    shuffled = rng.permutation(indices)
    order = np.lexsort((candidates[shuffled], entities[shuffled] // 16))
    ordered = shuffled[order]
    batches = [ordered[begin:begin + size] for begin in range(0, len(ordered), size)]
    return [batches[i] for i in rng.permutation(len(batches))]


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
    policy = Categorical(logits=logits)
    count = legal.sum(-1)
    entropy = policy.entropy() / count.clamp_min(2).float().log()
    return policy, entropy
