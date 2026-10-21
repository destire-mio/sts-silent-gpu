import json
import subprocess
from unittest.mock import patch

from tools.e4_continue import waiting


def test_running_gpu_shares_but_pending_resources_yields(tmp_path):
    node = ('NodeName=gpu01 State=MIXED Gres=gpu:V100:8 GresUsed=gpu:V100:1(IDX:0) '
            'CfgTRES=cpu=56,mem=128027M AllocTRES=cpu=2,mem=4G')
    for state, reason, expected in [('RUNNING', 'None', False), ('PENDING', 'Resources', True)]:
        job = (f'JobId=3684 JobName=other UserId=other(1013) JobState={state} Reason={reason} '
               'Dependency=(null) TimeLimit=1-00:00:00 StartTime=Unknown RunTime=00:00:00 '
               'NodeList=gpu01 TresPerNode=gres:gpu:V100:1 TRES=cpu=1,mem=2G,node=1')
        with patch.dict('os.environ', USER='zhuyouwei'), \
                patch('tools.e4_continue.subprocess.check_output', side_effect=[job, node]), \
                patch('tools.e4_continue.dist.broadcast_object_list'):
            assert waiting(0, tmp_path, 111) is expected
        snapshot = json.loads((tmp_path / 'queue-last.json').read_text())
        assert snapshot['blocking_job_ids'] == (['3684'] if expected else [])


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
