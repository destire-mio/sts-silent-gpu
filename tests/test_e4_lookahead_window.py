import json
from types import SimpleNamespace
import pytest
from train.e4 import REACH, PHASE, COMBAT_PHASE
from train.e4_stage12 import NoSLSearch
from train.e4_lookahead import LookaheadSearch
from train.e4_lookahead_window import summary, combat_estimate, lookahead60_window, window_stop_requested


def test_same_seed_paired_summary_and_combat_window_limit():
    rows = []
    for seed,floor in [(1,12),(2,18)]:
        reach = dict.fromkeys(REACH,False)
        reach.update(act1_boss=True,act2_boss=seed==2,act3_second_boss=seed==2,heart=seed==2)
        rows.append(dict(seed=seed,floor=floor,reach=reach))
    row = summary(rows,[dict(seed=2,floor=20),dict(seed=1,floor=10)])
    assert row['mean_floor'] == 15 and row['paired_mean_delta'] == 0
    assert row['paired_SE'] == pytest.approx(2.)
    assert row['act1_boss'] == 2 and row['act2_boss'] == 1
    assert row['victories'] == row['heart_wins'] == 1
    assert combat_estimate(900) == 3600
    assert combat_estimate(901) > 3600


def test_preview_and_override_fractions_count_actual_mainline_decisions(monkeypatch):
    search = object.__new__(LookaheadSearch)
    for name in ('mainline_decisions','preview_decisions','overrides','preview_steps'):
        setattr(search,name,0)
    search.failed = {}
    progress = {}
    for slot in range(3):
        row = [0] * (PHASE+1)
        row[PHASE] = COMBAT_PHASE if slot < 2 else 0
        progress[slot] = row
    search.progress_for = lambda slots:progress
    monkeypatch.setattr(NoSLSearch,'predict',lambda self,model,slots:([0,1,0],[.1,.2,.3],['a','b','c']))
    search.lookahead = SimpleNamespace(select=lambda model,slots:([1,1],[
        dict(preview_steps=32,candidates=[0,1],selected=1),
        dict(preview_steps=0,candidates=[1],selected=1)]))
    actions,values,logs = search.predict(None,[0,1,2])
    assert actions == [1,1,0] and values == [.1,.2,.3] and logs == ['a','b','c']
    assert search.mainline_decisions == 3 and search.preview_decisions == search.overrides == 1
    assert search.preview_steps == 32
    search.predict(None,[0,1,2])
    assert search.mainline_decisions == 6 and search.preview_decisions == search.overrides == 2


def test_supplement_obeys_existing_yield_receipt_and_allocation_deadline(tmp_path,monkeypatch):
    monkeypatch.setattr('train.e4_lookahead_window.time.monotonic',lambda:10.)
    assert not window_stop_requested(tmp_path,11.)
    assert window_stop_requested(tmp_path,10.)
    (tmp_path/'stop-requested').touch()
    assert window_stop_requested(tmp_path,11.)


@pytest.mark.parametrize("world", [1, 2])
def test_legacy_lookahead_is_skipped_on_small_allocations(tmp_path, world, capsys):
    assert lookahead60_window(tmp_path, None, None, 0, world, None)
    assert capsys.readouterr().out == ""
    (tmp_path / "dev-60.pt").touch()

    for rank in range(world):
        assert lookahead60_window(tmp_path, None, None, rank, world, None)

    assert json.loads(capsys.readouterr().out) == {
        "event": "lookahead60_skipped", "world": world, "reason": "requires_eight_ranks",
    }
    assert not (tmp_path / "claude137-dev60").exists()
