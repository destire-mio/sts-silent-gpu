import gzip
import json

from train.e4 import ACT, BOSS_ROOM, COMBAT, COMBAT_PHASE, FLOOR, HP, KEYS, PHASE, ROOM, SCREEN
from train.e4_late_pools import late_bank_counts, read_e2_late_roots, replay_matches


def test_e2_train_roots_and_heart_turn_boundaries(tmp_path):
    root = {"seed": 1_000_000_001, "candidates": [0]}
    row = dict(partition="train", stage="heart_turn3", root=root, keys=[True, True, True],
               floor=55, hp=50, turn=3)
    path = tmp_path / "roots.jsonl.gz"
    with gzip.open(path, "wt", encoding="utf-8") as stream:
        for item in (row, dict(row, partition="dev"), dict(row, stage="act3_entry")):
            stream.write(json.dumps(item) + "\n")
    groups = read_e2_late_roots(path)
    assert groups["heart_mid"] == [row]
    assert sum(map(len, groups.values())) == 1
    progress = {ACT: 4, ROOM: BOSS_ROOM, PHASE: COMBAT_PHASE, SCREEN: COMBAT, FLOOR: 55, HP: 50, KEYS: 7}
    assert replay_matches(row, progress, 3)
    assert not replay_matches(row, progress, 5)


def test_generated_roots_replace_half_and_external_fills_remainder():
    assert late_bank_counts(256, 0) == (256, 0)
    assert late_bank_counts(256, 32) == (224, 32)
    assert late_bank_counts(256, 128) == (128, 128)
