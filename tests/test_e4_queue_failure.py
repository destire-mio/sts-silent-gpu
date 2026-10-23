import json
import subprocess
from unittest.mock import patch

from tools.e4_continue import waiting


def test_queue_reason_policy_and_query_failures(tmp_path):
    node = ('NodeName=gpu01 State=MIXED Gres=gpu:V100:8 GresUsed=gpu:V100:1(IDX:0) '
            'CfgTRES=cpu=56,mem=128027M AllocTRES=cpu=2,mem=4G')
    reasons = ('Resources', 'Priority', 'JobArrayTaskLimit', 'Dependency',
               'DependencyNeverSatisfied', 'BeginTime', 'JobHeldUser', 'JobHeldAdmin',
               'QOSMaxJobsPerUser', 'QOSMaxGRESPerUser', 'AssocMaxJobsLimit',
               'AssocMaxGRESPerJob')
    cases = [('PENDING', reason, 'other', reason in ('Resources', 'Priority'))
             for reason in reasons]
    cases += [('RUNNING', 'Resources', 'other', False),
              ('PENDING', 'Resources', 'zhuyouwei', False),
              ('PENDING', 'Priority', 'zhuyouwei', False)]
    for version, (state, reason, user, expected) in enumerate(cases):
        job = (f'JobId=3684 JobName=other UserId={user}(1013) JobState={state} Reason={reason} '
               'Dependency=(null) TimeLimit=1-00:00:00 StartTime=Unknown RunTime=00:00:00 '
               'NodeList=gpu01 TresPerNode=gres:gpu:V100:1 TRES=cpu=1,mem=2G,node=1')
        with patch.dict('os.environ', USER='zhuyouwei'), \
                patch('tools.e4_continue.subprocess.check_output', side_effect=[job, node]), \
                patch('tools.e4_continue.dist.broadcast_object_list') as broadcast:
            assert waiting(0, tmp_path, version) is expected
        broadcast.assert_called_once_with([expected], src=0)
        snapshot = json.loads((tmp_path / 'queue-last.json').read_text())
        assert snapshot['blocking_job_ids'] == (['3684'] if expected else [])
        assert (tmp_path / f'queue-yield-{version}.json').exists() is expected
    for version, outputs in enumerate((
            [subprocess.CalledProcessError(1, ['scontrol', 'show', 'jobs', '-o'])],
            [job, subprocess.CalledProcessError(1, ['scontrol', 'show', 'node', 'gpu01', '-o', '-d'])],
            [subprocess.TimeoutExpired(['scontrol', 'show', 'jobs', '-o'], 60)],
            ['JobId=1'],
            [job, 'NodeName=gpu01']), start=100):
        with patch.dict('os.environ', USER='zhuyouwei'), \
                patch('tools.e4_continue.subprocess.check_output', side_effect=outputs), \
                patch('tools.e4_continue.dist.broadcast_object_list') as broadcast:
            assert waiting(0, tmp_path, version) is True
        broadcast.assert_called_once_with([True], src=0)
        evidence = json.loads((tmp_path / f'queue-error-{version}.json').read_text())
        assert evidence['error_type'] in ('CalledProcessError', 'TimeoutExpired', 'ValueError')
