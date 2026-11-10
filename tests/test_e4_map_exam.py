import torch

from tools.e4_map_exam import evaluate, markdown
from tools.e4_value_truth import collector


class EndTurnPolicy(torch.nn.Module):
    def forward(self, inputs):
        logits = (inputs["candidate_kinds"] == 2).float()
        return logits, logits.new_zeros(len(logits))


def test_map_exam_native_cpu_smoke():
    torch.set_num_threads(1)
    device = torch.device("cpu")
    model = EndTurnPolicy()
    seeds = [2_000_000_000, 2_000_000_001]
    baseline = collector(2, device, 130, float("inf"), greedy=True).evaluate(model, seeds)
    result = evaluate(model, seeds, {row["seed"]: row for row in baseline}, device, 2, 2, limit=3)
    assert [row["seed"] for row in result["episodes"]] == seeds
    assert result["comparison"]["seeds"] == seeds
    assert result["preview_decisions"] > 0 and result["failed_previews"] == 0
    assert result["average_steps_per_decision"] >= 4
    assert "act3_first_boss" in markdown(result)
