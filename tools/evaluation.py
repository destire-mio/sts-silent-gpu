"""Root-family partitions and fixed-sample evaluation arithmetic; no training."""
import json
import math
from pathlib import Path

PARTITIONS = json.loads((Path(__file__).resolve().parents[1] / "inventory/seed-partitions.json").read_text())


def partition(root_seed):
    for name in ("train", "dev", "locked_test"):
        if any(start <= root_seed <= end for start, end in PARTITIONS[name]):
            return name
    raise ValueError("Root seed is outside the committed partitions")


def wilson(wins, total, z=1.959963984540054):
    if not isinstance(wins, int) or not isinstance(total, int) or not 0 <= wins <= total or total == 0 or z <= 0:
        raise ValueError("Wilson requires integer 0 <= wins <= total, total > 0 and z > 0")
    p = wins / total
    denominator = 1 + z * z / total
    center = (p + z * z / (2 * total)) / denominator
    half = z * math.sqrt(p * (1 - p) / total + z * z / (4 * total * total)) / denominator
    return max(0.0, center - half), min(1.0, center + half)


def paired_counts(reference, candidate):
    """Maps root seed to victory/death/truncated/fault; one attempt per root."""
    if reference.keys() != candidate.keys() or not reference:
        raise ValueError("Paired evaluation requires identical nonempty root-seed sets")
    allowed = {"victory", "death", "truncated", "fault"}
    if any(value not in allowed for value in [*reference.values(), *candidate.values()]):
        raise ValueError("Unknown episode outcome")
    result = dict(roots=len(reference), both_victory=0, both_death=0, candidate_only=0,
                  reference_only=0, fault_pairs=0, truncated_pairs=0)
    for seed, old in reference.items():
        new = candidate[seed]
        if "fault" in (old, new):
            result["fault_pairs"] += 1
        elif "truncated" in (old, new):
            result["truncated_pairs"] += 1
        elif old == new:
            result["both_" + old] += 1
        elif new == "victory":
            result["candidate_only"] += 1
        else:
            result["reference_only"] += 1
    return result
