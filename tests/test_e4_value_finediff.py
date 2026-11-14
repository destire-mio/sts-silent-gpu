from copy import deepcopy
import json
import time

import pytest
import torch

from tools.e4_value_finediff import audit_roots, report
from tools.e4_value_truth import EXAM_SEEDS, RootCollector, collector


class EndTurnPolicy(torch.nn.Module):
    def forward(self, inputs):
        kinds = inputs["candidate_kinds"]
        logits = torch.where(kinds == 2, 100., 0.).masked_fill(~inputs["candidate_mask"], -torch.inf)
        return logits, torch.full((len(kinds),), .25)


def test_value_finediff_cpu_two_roots():
    native = pytest.importorskip("spire_finediff")
    torch.set_num_threads(1)
    model = EndTurnPolicy()
    runner = collector(2, torch.device("cpu"), 101, time.monotonic() + 90, greedy=True,
                       storage_slots=10, cls=RootCollector)
    search = runner.search
    seeds = list(EXAM_SEEDS[:2])
    runner.reset([0, 1], seeds)
    original = search.take(2)
    search.clone([0, 1], original)
    progress = search.progress_for(original)
    roots = [dict(slot=slot, seed=seed, progress=progress[slot], ledger=deepcopy(runner.rewards[i]))
             for i, (seed, slot) in enumerate(zip(seeds, original))]
    original_bytes = [search.batch.state_bytes(slot) for slot in original]
    variant = search.take(2)
    search.clone(original, variant)
    assert native.perturb_slots(search.batch, variant, 0) == [True, True]
    for base, changed in zip(original, variant):
        assert native.equal_except_hp(search.batch, base, changed, 5)
    search.available.extend(variant)

    result = audit_roots(runner, model, {"fixture": roots}, 2, 221)
    assert [search.batch.state_bytes(slot) for slot in original] == original_bytes
    group = result["nodes"]["fixture"]
    assert group["control"]["complete_roots"] == 2
    controls = {row["seed"]: row["futures"] for row in group["control"]["states"]}
    for change in ("hp_minus_5", "hp_minus_10", "add_injury"):
        row = group["changes"][change]
        assert row["complete_pairs"] == 2 and row["paired_futures"] == 4
        for variant in row["variants"]:
            assert [f["seed"] for f in variant["futures"]] == [f["seed"] for f in controls[variant["seed"]]]
    assert group["changes"]["remove_potion"]["eligible_roots"] == 0
    assert len(group["changes"]["remove_potion"]["skipped"]) == 2
    assert "Mean ΔV" in report(result)
    json.dumps(result, allow_nan=False)
