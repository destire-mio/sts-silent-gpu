import json
from contextlib import redirect_stdout
from io import StringIO
from unittest.mock import patch

import torch

from train import e4_expert_window as window


def test_missing_expert_snapshot_clears_pending(tmp_path):
    pending = tmp_path / "expert-pending-0.json"
    pending.write_text(json.dumps({"hour": 8}))
    other = tmp_path / "expert-deferred-16.json"
    other.write_text(json.dumps({"hour": 16}))
    log = StringIO()

    with patch.object(window.dist, "broadcast") as broadcast, redirect_stdout(log):
        assert window.periodic_combat_window(tmp_path, None, torch.device("cpu"), 0, 1, float("inf"))
    assert broadcast.call_args.args[0].tolist() == [8, 1]
    assert not pending.exists()
    assert other.is_file()
    assert json.loads(log.getvalue()) == dict(event="expert_window_skipped", hour=8,
        reason="missing_dev_checkpoint", checkpoint=str(tmp_path / "dev-8.pt"))
