import json
import os
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch


from tools import e4_stage12_chain as chain


class ChainTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        (self.root / 'training').mkdir()
        self.state = dict(limit_node_seconds=29399, charged_node_seconds=14400,
            request=dict(world=8, cpus=9, memory_mib=21649), training_root=str(self.root),
            repository='/repo', next_resource_check_at=time.time() + 630, resource_triggers=[],
            resources=dict(finished_at=time.time(), own_queue='10|RUNNING|parent\n', pending=[],
                           node='CPUTot=56 RealMemory=128000 Gres=gpu:V100:8'),
            jobs={'10': dict(world=8, wall_seconds=14400, reserved_node_seconds=14400,
                             state='RUNNING', settled=False)})
        (self.root / 'state.json').write_text(json.dumps(self.state))

    def tearDown(self):
        self.temporary.cleanup()

    def test_reserve_before_release_budget_tail_and_idempotent_queue(self):
        submitted = []
        def command(*args, **_):
            if args[0] == 'sacct':
                identity = args[2]
                return identity + ('|RUNNING|0:0|0||\n' if identity == '10' else '|PENDING|0:0|0||\n')
            if args[0] == 'sbatch':
                submitted.append(args)
                self.assertIn('--dependency=afterany:10', args)
                self.assertIn('--time=00:09:00', args)
                return 'Submitted batch job 11\n'
            if args[:3] == ('scontrol', 'show', 'job'):
                return ('JobId=11 UserId=zhuyouwei(1017) JobState=PENDING Priority=0 Reason=JobHeldUser '
                    'NumNodes=1-1 NumCPUs=9 Partition=N56_128G_V100 ReqNodeList=gpu01 '
                    'MinMemoryNode=21649M TresPerNode=gres:gpu:V100:8 TimeLimit=00:09:00 Dependency=afterany:10(unfulfilled)')
            if args[:2] == ('scontrol', 'release'):
                saved = json.loads((self.root / 'state.json').read_text())
                self.assertEqual(saved['jobs']['11']['reserved_node_seconds'], 540)
                self.assertLessEqual(saved['charged_node_seconds'] + chain.reservation(saved), 29399)
                return ''
            self.fail(args)
        proof = dict(at=time.time(), source_commit='source', files=1, path='proof', sha256='digest')
        with patch.object(chain, 'command', command), patch.object(chain, 'fullhash', return_value=proof):
            self.assertEqual(chain.enqueue(self.root, '10'), 11)
            self.assertEqual(chain.enqueue(self.root, '10'), 11)
        self.assertEqual(len(submitted), 1)

    def test_terminal_charge_exact_gpu_fraction_and_only_once(self):
        self.state['jobs']['10'].update(world=7, wall_seconds=1000, reserved_node_seconds=875)
        (self.root / 'final-10.txt').write_text('JOB_GRES=gpu:V100:7')
        (self.root / 'memory-10.json').write_text(json.dumps(dict(peak_bytes=1048576)))
        raw = '10|COMPLETED|0:0|101|start|end\n'
        with patch.object(chain, 'command', return_value=raw):
            chain.settle(self.root, self.state)
            chain.settle(self.root, self.state)
        self.assertEqual(self.state['charged_node_seconds'], 14400 + 101 * 7 / 8)
        self.assertEqual(chain.reservation(self.state), 0)
        events = [json.loads(line) for line in (self.root / 'events.jsonl').read_text().splitlines()]
        self.assertEqual(len(events), 1)

    def test_health_stop_is_not_cleared_and_has_no_successor(self):
        self.state['jobs']['11'] = dict(world=8, wall_seconds=14400, reserved_node_seconds=14400,
                                        state='PENDING', settled=False)
        self.state['jobs']['10'].update(settled=True, reserved_node_seconds=0)
        (self.root / 'state.json').write_text(json.dumps(self.state))
        checkpoint = dict(state=dict(stop_reason='NaN_or_Inf'), optimizer_steps=123,
                          partial_steps=0, partial_attempts=0, partial_skips=0)
        def command(*args, **_):
            return '10|COMPLETED\n' if args[-1] == '--format=JobIDRaw,State' else '11|RUNNING|0:0|0||\n'
        with patch.dict(os.environ, SLURM_JOB_ID='11', STAGE12_PARENT='10'), \
             patch.object(chain, 'command', command), \
             patch.object(chain, 'checkpoint_state', return_value=checkpoint), \
             patch.object(chain, 'enqueue') as enqueue, patch.object(chain.subprocess, 'Popen') as worker:
            self.assertEqual(chain.run(self.root), 0)
            enqueue.assert_not_called()
            worker.assert_not_called()
        saved = json.loads((self.root / 'state.json').read_text())
        self.assertTrue(saved['halted'])
        self.assertEqual(saved['jobs']['11']['resume']['state']['stop_reason'], 'NaN_or_Inf')

    def test_yield_waits_for_other_users_without_charging_a_new_reservation(self):
        self.state['jobs']['11'] = dict(parent_job=10, world=8, wall_seconds=14400,
            reserved_node_seconds=14400, state='PENDING', settled=False)
        self.state['resources']['pending'] = [dict(JobId='42', Reason='Resources')]
        (self.root / 'state.json').write_text(json.dumps(self.state))
        calls = []
        def command(*args, **_):
            calls.append(args)
            if args[0] == 'sacct':
                return args[2] + ('|RUNNING|0:0|0||\n' if args[2] == '10' else '|PENDING|0:0|0||\n')
            self.assertEqual(args, ('scontrol', 'update', 'JobId=11', 'Dependency=afterany:10:42'))
            return ''
        with patch.object(chain, 'command', command):
            result = chain.watch(self.root, 'heartbeat-test')
        self.assertFalse(result['queried'])
        self.assertTrue((self.root / 'training/stop-requested').exists())
        self.assertEqual(chain.reservation(result['state']), 28800)
        self.assertFalse(any(args[0] == 'sbatch' for args in calls))


if __name__ == '__main__':
    unittest.main()
