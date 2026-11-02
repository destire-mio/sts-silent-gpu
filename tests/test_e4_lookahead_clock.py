import pytest
from train.e4 import AllocationEnded, FLOOR, SCREEN
from train.e4_stage12 import NoSLSearch
from train.e4_lookahead import LookaheadSearch


def make_search():
    search = object.__new__(LookaheadSearch)
    search.lanes = 32
    search.last_heartbeat = -100
    search.evidence_directory = None
    search.provenance = {0:(123, [1, 2])}
    row = [0] * (max(FLOOR, SCREEN)+1)
    row[FLOOR] = 7
    search.mainline_progress = {0:row, 720:row.copy()}
    return search


def test_preview_cache_is_excluded_from_real_mainline_heartbeat(monkeypatch):
    search = make_search()
    original = search.mainline_progress
    monkeypatch.setattr(NoSLSearch.__mro__[1], 'clock', lambda self:'base clock')
    assert search.clock() == 'base clock'
    assert [x['lane'] for x in search.actual_lanes] == [0]
    assert search.actual_lanes[0]['seed'] == 123
    assert search.mainline_progress is original and 720 in original


def test_existing_clock_stop_propagates_and_restores_preview_cache(monkeypatch):
    search = make_search()
    original = search.mainline_progress
    def stop(self):
        raise AllocationEnded()
    monkeypatch.setattr(NoSLSearch.__mro__[1], 'clock', stop)
    with pytest.raises(AllocationEnded):search.clock()
    assert search.mainline_progress is original and 720 in original
