from copy import deepcopy
import json
import time

import pytest
import torch

from tools.e4_critic_fit import collect_returns, fit_critic, load_critic, read_shard, write_json
from tools.e4_value_finediff import audit_roots
from tools.e4_value_truth import EXAM_SEEDS, RootCollector, collector
from train.e4_stage12 import CLSPolicy
from train.model import ModelConfig, network_inputs


class EndTurnPolicy(CLSPolicy):
    def action_logits(self, observation, entities, pooled):
        logits = torch.where(observation["candidate_kinds"] == 2, 100., 0.)
        return logits.masked_fill(~observation["candidate_mask"], -torch.inf)


def test_offline_critic_cpu_collect_fit_and_audit(tmp_path):
    pytest.importorskip("spire_finediff")
    torch.set_num_threads(1)
    torch.manual_seed(101)
    device = torch.device("cpu")
    policy = EndTurnPolicy(ModelConfig(width=16, layers=1, heads=2, feedforward=32)).eval()
    original_policy = deepcopy(policy.state_dict())
    directory = tmp_path / "returns"
    seeds = range(1_000_000_000, 1_000_000_002)
    manifest = collect_returns(policy, seeds, directory, device, lanes=2, every=8,
                               max_points=2_000_000, seed=101)
    manifest["policy_sha256"] = "fixture"
    write_json(directory / "manifest.json", manifest)
    assert manifest["episodes"] == manifest["outcomes"]["death"] == 2
    assert manifest["key_last_chance"] is False
    reference = collector(2, device, 101, time.monotonic() + 60, greedy=True)
    games = {row["seed"]: row for row in reference.evaluate(policy, seeds)}
    for split, seed in (("validation", seeds[0]), ("train", seeds[1])):
        assert manifest["rows"][split] > 1
        shard = read_shard(directory / manifest["shards"][split][0], device)
        assert set(shard["seeds"].tolist()) == {seed}
        game = games[seed]
        assert len(shard["targets"]) == (len(game["candidates"]) + 7) // 8
        assert shard["targets"][0].item() == pytest.approx(game["reward"])
        assert torch.all((shard["targets"] >= 0) & (shard["targets"] <= 1))

    def audit(critic=None):
        runner = collector(1, device, 101, time.monotonic() + 60, greedy=True,
                           storage_slots=5, cls=RootCollector)
        runner.reset([0], EXAM_SEEDS[:1])
        slot, = runner.search.take(1)
        runner.search.clone([0], [slot])
        progress = runner.search.progress_for([slot])[slot]
        root = dict(slot=slot, seed=EXAM_SEEDS[0], progress=progress, ledger=deepcopy(runner.rewards[0]))
        result = audit_roots(runner, policy, {"fixture": [root]}, 2, 223, critic=critic)
        if critic is not None:
            inputs = network_inputs(runner.search.arrays, device, torch.tensor([slot]))
            with torch.no_grad():
                expected = critic(inputs).item()
            assert result["nodes"]["fixture"]["control"]["states"][0]["value"] == expected
        return result["nodes"]["fixture"]["control"]["states"][0]

    control = audit()
    for loss in ("mse", "hlgauss"):
        model_dir = tmp_path / loss
        fit_critic(policy, directory, model_dir, device, loss=loss, steps=2, batch_size=4,
                   eval_every=1, lr=3e-5, seed=101, policy_sha256="fixture")
        critic = load_critic(model_dir / "critic.pt", device)
        assert not torch.equal(critic.policy.cls, original_policy["cls"])
        metrics = json.loads((model_dir / "fit.json").read_text(encoding="utf-8"))["metrics"]
        assert [row["step"] for row in metrics] == [1, 2]
        assert all(row["validation_rows"] == manifest["rows"]["validation"] for row in metrics)
        audited = audit(critic)
        assert audited["complete_futures"] == 2
        assert audited["futures"] == control["futures"]
        assert audited["value"] != control["value"]

    assert all(torch.equal(value, original_policy[name]) for name, value in policy.state_dict().items())

