"""Focused CPU checks for #142 comparison size and matched completion statistics."""
import pytest
from train.e4_expert_window import (COMPARISON_EPISODES, complete_seed_set,
                                    paired_completed_summary)


def episode(seed, floor):
    return dict(seed=seed, floor=floor, reach=dict(act1_boss=floor>=17,
        act2_boss=floor>=34, act3_first_boss=floor>=51,
        act3_second_boss=floor>=52, heart=floor>=57))


def test_comparison_uses_exact_first128_and16_per_rank():
    assert COMPARISON_EPISODES == 128
    rows = [episode(seed, 1) for seed in range(2000000000,2000000128)]
    assert complete_seed_set(rows)
    for rank in range(8):
        assert len(rows[rank::8]) == 16 and complete_seed_set(rows[rank::8], rank)
    assert not complete_seed_set(rows+[episode(2000000128,1)])
    assert not complete_seed_set(rows[:-1]+[rows[0]])


def test_cap_excludes_missing_from_both_modes_and_pairs_by_seed():
    baseline = [episode(seed, 99) for seed in range(2000000000,2000000128)]
    baseline[0] = episode(2000000000,10)
    baseline[1] = episode(2000000001,30)
    combat = [episode(2000000001,29),episode(2000000000,13)]
    result = paired_completed_summary(baseline, combat)
    assert result['episodes'] == 2
    assert result['excluded_seeds'] == list(range(2000000002,2000000128))
    assert result['modes']['argmax']['mean_floor'] == 20
    assert result['modes']['combat']['mean_floor'] == 21
    assert result['modes']['combat']['paired_mean_delta'] == 1
    assert result['modes']['combat']['paired_SE'] == pytest.approx(2)
    assert result['modes']['argmax']['act1_boss'] == 1
    assert result['modes']['combat']['act1_boss'] == 1
    assert result['modes']['argmax']['victories'] == 0


def test_no_or_single_completed_pair_does_not_invent_standard_error():
    baseline = [episode(seed, 4) for seed in range(2000000000,2000000128)]
    empty = paired_completed_summary(baseline, [])
    assert empty['episodes'] == 0 and empty['modes'] == {}
    one = paired_completed_summary(baseline, [episode(2000000003,7)])
    assert one['episodes'] == 1
    assert one['modes']['combat']['paired_mean_delta'] == 3
    assert one['modes']['combat']['paired_SE'] is None
