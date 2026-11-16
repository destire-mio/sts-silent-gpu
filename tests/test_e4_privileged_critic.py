import gzip
import json
from pathlib import Path

import pytest
import torch

from train.e4_stage12 import CLSPolicy, NoSLCollector, NoSLSearch, SeparateCritic
from train.model import ModelConfig, network_inputs


def test_training_critic_reads_hidden_order_without_changing_policy(monkeypatch):
    spire = pytest.importorskip("spire")
    torch.set_num_threads(1)
    torch.manual_seed(233)
    device = torch.device("cpu")
    search = NoSLSearch(2, device, 130, greedy=True, probe_slots=1)
    collector = NoSLCollector(search, torch.Generator().manual_seed(130))

    trace = Path(__file__).parent / "traces" / "Transmogrifier-curse-preview-seed2000000764.jsonl.gz"
    prefix = []
    with gzip.open(trace, "rt", encoding="utf-8") as stream:
        seed = json.loads(next(stream))["seed"]
        for line in stream:
            row = json.loads(line)
            prefix.append(row["legal"].index(row["action"]))
            state = row["snapshot"]
            if state["run"]["floorNum"] == 15 and state["screen"]["type"] == "MAP" and row["action"]["type"] == "choose":
                break

    collector.reset([0, 1], [seed, seed])
    search.batch.reset_to(0, seed, prefix)
    search.batch.clone_many([0], [1])
    search.batch.determinize_slots([0, 1], [752, 947])
    for slot in (0, 1):
        collector.paths[slot] = prefix.copy()
        search.provenance[slot] = (seed, prefix.copy())
    search.reset_seen([0, 1])

    policy = CLSPolicy(ModelConfig(32, 1, 4, 64)).eval()
    ordinary = SeparateCritic(policy).eval()
    private = SeparateCritic(policy, privileged_draw_slots=spire.PRIVILEGED_DRAW_SLOTS).eval()
    search.training_critic = private
    search.privileged_token_slots = spire.PRIVILEGED_TOKEN_SLOTS
    tokens = search.read_privileged_tokens([0, 1])
    draw = tokens[:, :spire.PRIVILEGED_DRAW_SLOTS]
    assert not torch.equal(draw[0], draw[1])
    assert torch.equal(draw[0].sort().values, draw[1].sort().values)
    public = network_inputs(search.arrays, device, torch.arange(2))
    assert all(torch.equal(tensor[0], tensor[1]) for tensor in public.values())
    combat = torch.ones(2, dtype=torch.bool)
    with torch.no_grad():
        logits, public_value = policy(public)
        left, right = [policy({name: tensor[i:i+1] for name, tensor in public.items()})[0] for i in (0, 1)]
        assert torch.equal(left, right)
        raw = ordinary.values(ordinary.encode(public)[1], combat)
        assert torch.equal(ordinary(public, combat), raw)
        assert torch.equal(private(public, combat), raw)
        private_values = private(public, combat, privileged_tokens=tokens)
        assert private_values[0] != private_values[1]
        assert torch.equal(policy(public)[0], logits)
        with pytest.raises(AssertionError):
            policy({**public, "privileged_tokens": tokens})

    fragment = collector.collect(policy, 1)
    assert torch.equal(fragment["privileged_tokens"], tokens)
    assert torch.equal(fragment["values"], private_values)
    assert torch.equal(fragment["public_rollout_values"], public_value)
    assert search.privileged_rollout is None
    monkeypatch.setattr(search, "read_privileged_tokens", lambda *args: pytest.fail("private data requested in exam path"))
    search.scores(policy, [0, 1], search.progress_for([0, 1]))

