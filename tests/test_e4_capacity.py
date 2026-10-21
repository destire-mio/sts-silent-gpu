from collections import deque
from types import SimpleNamespace
from unittest.mock import Mock
import json

import pytest
import torch

from train.e4 import CAPACITY_METRICS, COMBAT_PHASE, PHASE, RoomSearch, SEARCH_METRICS


def test_combat_capacity_excludes_only_faulted_trial(tmp_path):
    search = object.__new__(RoomSearch)
    search.lanes, search.trials = 1, 2
    search.device = SimpleNamespace(index=0)
    search.available, search.contexts = [1, 2], []
    search.metrics = dict.fromkeys(SEARCH_METRICS, 0)
    search.capacity_metrics = dict.fromkeys(CAPACITY_METRICS, 0)
    search.played_cards_capacity_fault = 0x43415031
    search.provenance = {s: (2000000544, []) for s in range(3)}
    search.arrays = dict(faults=torch.zeros(3, dtype=torch.int64), truncated=torch.zeros(3, dtype=torch.bool))
    search.evidence_directory = tmp_path
    search.searched_decisions, search.combat_tails = 0, {}
    search.clone = Mock()
    search.clock = Mock()
    search.predict = Mock(return_value=([0, 0], [], []))
    search.trace_step = lambda context, slots, choices: None
    progress = [[0] * 9 for _ in range(3)]
    progress[1][PHASE] = COMBAT_PHASE
    search.progress = Mock(return_value=progress)
    search.scores = Mock(return_value={2: .5})

    def step(slots, choices, context):
        search.arrays['faults'][1] = search.played_cards_capacity_fault
        context['tail'][1] = deque([dict(screen='combat', candidate=0, action_kind='play')], maxlen=30)
        return search.check(slots, context)

    search.step = step
    assert search.combat(None, [0]) == {0: [0]}
    search.scores.assert_called_once_with(None, [2], progress)
    assert search.clone.call_args.args == ([2], [0])
    assert search.capacity_metrics == dict(combat_played_cards_limit=1, combat_trials=2)
    evidence = json.loads((tmp_path / 'combat-played-cards-limit-0.jsonl').read_text())
    assert evidence['seed'] == 2000000544 and evidence['slot'] == 1 and len(evidence['last30']) == 1
    with pytest.raises(AssertionError, match='E4 engine fault'):
        search.check([1])
    search.arrays['faults'][1] = 123
    context = search.begin_search('combat', [1])
    with pytest.raises(AssertionError, match='E4 engine fault'):
        search.check([1], context)
