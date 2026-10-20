import json

import torch
import spire

from train.e4 import COMBAT_PHASE, PHASE, RoomSearch
from train.model import ModelConfig, Policy


def check():
    device = torch.device("cuda", 0)
    torch.cuda.set_device(device)
    torch.manual_seed(1729)
    model = Policy(ModelConfig()).to(device).eval()
    rng = torch.Generator(device=device).manual_seed(1729)
    search = RoomSearch(1, 8, device, rng, scratch_slots=256)
    seed = 1000000000
    search.batch.reset_to(0, seed, [])
    prefix = []
    checks = []
    for kind in ("event", "combat"):
        if kind == "combat":
            while search.progress()[0][PHASE] != COMBAT_PHASE:
                choices, _, _ = search.predict(model, [0])
                search.step([0], choices)
                prefix.extend(choices)
        selected = search.event(model, [0])[0] if kind == "event" else search.combat(model, [0])[0]
        prefix.extend(selected)
        replay = spire.TrainingBatch(1, 1, 2**63 - 1, [])
        replay.reset_to(0, seed, [])
        for action in prefix:
            replay.step_slots([0], [action])
            assert not replay.faults[0]
        assert search.batch.state_bytes(0) == replay.state_bytes(0), kind
        checks.append(dict(kind=kind, prefix_decisions=len(prefix), selected_decisions=len(selected),
                           full_game_and_history_equal=True))
    print(json.dumps(dict(seed=seed, checks=checks)))


if __name__ == "__main__":
    check()
