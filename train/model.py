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
        result[name] = torch.tensor(arrays[name][slots][..., :width], device=device, dtype=dtype)
    result["entity_mask"] = torch.tensor(entity_mask[:, :entities], device=device)
    result["candidate_mask"] = torch.tensor(candidate_mask[:, :candidates], device=device)
    return result


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
