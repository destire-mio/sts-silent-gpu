import json
import time

import torch

from tools.e4_macro_audit import audit, markdown, option_rows


class EndTurnPolicy(torch.nn.Module):
    def forward(self, inputs):
        kinds = inputs["candidate_kinds"]
        logits = torch.where(kinds == 2, 100., 0.).masked_fill(~inputs["candidate_mask"], -torch.inf)
        return logits, torch.full((len(kinds),), .25)


def test_macro_audit_native_cpu_runs_paired_determinized_options():
    torch.set_num_threads(1)
    assert option_rows("purge", [True]*3, [3,7,4], [1,0,2], [3,7], [1,1]) == [0,1]
    result = audit(EndTurnPolicy(), torch.device("cpu"), 8, 101, time.monotonic()+45,
                   quotas={("map", 1):1}, stages={"k8":2,"k32":2,"k128":2,"k32_independent":2},
                   limit=3, max_seeds=32)
    root, = result["roots"]
    assert result["complete"] and root["act"] == 1 and root["kind"] == "map" and len(root["options"]) > 1
    draws = []
    for name, stage in root["trials"].items():
        assert len(stage["seeds"]) == 2 and len(stage["scores"]) == len(root["options"])
        assert all(len(row) == 2 and all(value is not None for value in row) for row in stage["scores"].values())
        assert root["statistics"][name]["paired"] == 2
        assert root["timings"][name]["decisions"] == 6 * len(root["options"])
        draws.extend(stage["seeds"])
    assert len(set(draws)) == len(draws)
    assert "k32_vs_k32_independent" in markdown(result)
    assert all(row["first"] == .25 and row["room_entry"] == .25
               for row in root["option_values"].values())
    assert result["summary"]["map"]["successor_value"]["gain"] == 0.
    archived = json.loads(json.dumps(result, allow_nan=False))
    replay = audit(EndTurnPolicy(), torch.device("cpu"), 4, 101, time.monotonic()+45,
                   quotas={}, reference=archived)
    restored, = replay["roots"]
    assert replay["reused_k128"] and restored["trials"] == {"k128": root["trials"]["k128"]}
    assert restored["timings"] == {} and restored["option_values"] == root["option_values"]
