import json
import subprocess
from unittest.mock import patch

from tools.e4_continue import waiting


def test_scontrol_failure_yields(tmp_path):
    error = subprocess.CalledProcessError(1, ['scontrol', 'show', 'jobs', '-o'])
    with patch('tools.e4_continue.subprocess.check_output', side_effect=error) as command, \
            patch('tools.e4_continue.dist.broadcast_object_list') as broadcast:
        assert waiting(0, tmp_path, 109) is True
    command.assert_called_once_with(['scontrol', 'show', 'jobs', '-o'], text=True, timeout=60)
    broadcast.assert_called_once_with([True], src=0)
    evidence = json.loads((tmp_path / 'queue-error-109.json').read_text())
    assert evidence['error_type'] == 'CalledProcessError'
    assert evidence['error'] == str(error)
