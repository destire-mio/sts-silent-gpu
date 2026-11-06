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


def test_card_pairs_only_completed_intersection_even_if_baseline_is_capped():
    baseline = [episode(2000000000,10),episode(2000000001,20)]
    card = [episode(2000000001,24),episode(2000000002,99)]
    result = paired_completed_summary(baseline,card,'card')
    assert result['episodes'] == 1
    assert result['modes']['card']['paired_mean_delta'] == 4
    assert result['modes']['argmax']['mean_floor'] == 20
    assert 2000000000 in result['excluded_seeds'] and 2000000002 in result['excluded_seeds']


@pytest.mark.parametrize('hour,card_enabled', [(80, False), (88, True), (96, True), (104, True)])
def test_window_uses_run_randomized_card_from96h(tmp_path, monkeypatch, hour, card_enabled):
    from contextlib import nullcontext
    from types import SimpleNamespace
    import hashlib
    import json
    import torch
    import train.e4_expert_window as window
    (tmp_path/'expert-pending-0.json').write_text(json.dumps(dict(hour=hour)))
    (tmp_path/f'dev-{hour}.pt').write_bytes(b'fixed-exam-weights')
    monkeypatch.setattr(window.dist,'broadcast',lambda tensor,src:None)
    monkeypatch.setattr(torch.cuda,'synchronize',lambda device:None)
    monkeypatch.setattr(torch.cuda,'empty_cache',lambda:None)
    monkeypatch.setattr(torch.random,'fork_rng',lambda devices:nullcontext())
    loads = []
    def load(path, **kwargs):
        loads.append(path.name)
        return {}
    monkeypatch.setattr(torch,'load',load)
    class Policy:
        def __init__(self, config): pass
        def to(self, device): return self
        def load_state_dict(self, state, strict): assert strict
        def eval(self): return self
        def requires_grad_(self, flag): assert not flag
    monkeypatch.setattr(window,'CLSPolicy',Policy)
    class Search:
        def __init__(self, lanes, device, *args, **kwargs):
            self.lookahead = SimpleNamespace(rng=torch.Generator().manual_seed(152))
            self.clock = lambda:None
    class CardSearch(Search): pass
    monkeypatch.setattr(window,'ArgmaxSearch',Search)
    monkeypatch.setattr(window,'LookaheadSearch',Search)
    monkeypatch.setattr(window,'CardLookaheadSearch',CardSearch)
    class Collector:
        def __init__(self, search, rng, rank):
            self.search,self.completed = search,[]
        def evaluate(self, policy, seeds):
            card = isinstance(self.search,CardSearch)
            self.completed = [episode(seed,12 if card else 10) for seed in (seeds[:1] if card else seeds)]
            for row in self.completed:
                row.update(keys={},outcome='death',reward=0.)
            if card:
                self.search.card_reward_decisions += 5
                self.search.preview_decisions += 3
                self.search.overrides += 1
                self.search.preview_steps += 240
                raise window.AllocationEnded
    monkeypatch.setattr(window,'WindowCollector',Collector)
    clock = iter(range(0,1000000,1000))
    monkeypatch.setattr(window.time,'monotonic',lambda:float(next(clock)))
    assert window.periodic_combat_window(tmp_path,None,torch.device('cpu'),0,1,float('inf'))
    directory = tmp_path/f'claude138-dev{hour}'
    report = json.loads((directory/'report.json').read_text())
    assert set(report['modes']) == ({'argmax','combat','card'} if card_enabled else {'argmax','combat'})
    assert report['checkpoint_sha256'] == hashlib.sha256(b'fixed-exam-weights').hexdigest()
    assert report.get('card_randomization') == ('card 已用整局随机化（#157）' if hour >= 96 else None)
    assert loads == [f'dev-{hour}.pt']*(24 if card_enabled else 16) and report['parameter_updates'] == 0
    if not card_enabled:
        assert not list(directory.glob('card*'))
        assert set(report['paired_completed_by_mode']) == {'combat'}
        assert set(report['common_completed']['modes']) == {'combat'}
        assert report['common_completed']['episodes'] == 128
        return
    card = report['modes']['card']
    assert not card['complete'] and card['skipped'] == 'card_one_hour_cap'
    assert card['completed_episodes'] == 8
    assert card['counters']['card_reward_decisions'] == 40
    assert card['counters']['preview_decisions'] == 24 and card['counters']['overrides'] == 8
    assert card['counters']['preview_steps'] == 1920
    assert report['paired_completed_by_mode']['card']['modes']['card']['paired_mean_delta'] == 2
    assert report['common_completed']['episodes'] == 8
