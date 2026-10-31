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
            resources=dict(finished_at=time.time(), pending=[]),
            jobs={'10': dict(world=8, wall_seconds=14400, state='RUNNING', settled=False)})
        (self.root / 'state.json').write_text(json.dumps(self.state))
        self.environment = patch.dict(os.environ, USER='tester',
            STAGE12_PARTITION='test_partition', STAGE12_NODE='test_node')
        self.environment.start()

    def tearDown(self):
        self.environment.stop()
        self.temporary.cleanup()

    def test_direct_submit_budget_tail_and_idempotent_queue(self):
        submitted = []
        def command(*args, **_):
            if args[0] == 'sacct':
                return args[2] + '|RUNNING|0:0|0||\n'
            self.assertEqual(args[0], 'sbatch')
            submitted.append(args)
            self.assertIn('--dependency=afterany:10', args)
            self.assertIn('--time=00:09:00', args)
            self.assertIn('--partition=test_partition', args)
            self.assertIn('--nodelist=test_node', args)
            self.assertNotIn('--hold', args)
            return 'Submitted batch job 11\n'
        with patch.object(chain, 'command', command):
            self.assertEqual(chain.enqueue(self.root, '10'), 11)
            self.assertEqual(chain.enqueue(self.root, '10'), 11)
        self.assertEqual(len(submitted), 1)
        state = json.loads((self.root / 'state.json').read_text())
        self.assertNotIn('reserved_node_seconds', state['jobs']['11'])
        self.assertEqual(chain.available_wall(state), 0)

    def test_terminal_charge_exact_gpu_fraction_and_only_once(self):
        self.state['jobs']['10'].update(world=7, wall_seconds=1000, reserved_node_seconds=875)
        raw = '10|COMPLETED|0:0|101|start|end\n'
        with patch.object(chain, 'command', return_value=raw):
            chain.settle(self.root, self.state)
            chain.settle(self.root, self.state)
        self.assertEqual(self.state['charged_node_seconds'], 14400 + 101 * 7 / 8)
        self.assertEqual(self.state['jobs']['10']['reserved_node_seconds'], 0)
        events = [json.loads(line) for line in (self.root / 'events.jsonl').read_text().splitlines()]
        self.assertEqual(len(events), 1)

    def test_health_stop_is_not_cleared_and_has_no_successor(self):
        self.state['jobs']['11'] = dict(world=8, wall_seconds=14400, state='PENDING', settled=False)
        self.state['jobs']['10'].update(settled=True)
        (self.root / 'state.json').write_text(json.dumps(self.state))
        checkpoint = dict(state=dict(stop_reason='NaN_or_Inf'), optimizer_steps=123,
                          partial_steps=0, partial_attempts=0, partial_skips=0)
        with patch.dict(os.environ, SLURM_JOB_ID='11'), \
             patch.object(chain, 'command', return_value='11|RUNNING|0:0|0||\n'), \
             patch.object(chain, 'checkpoint_state', return_value=checkpoint), \
             patch.object(chain, 'enqueue') as enqueue, patch.object(chain.subprocess, 'Popen') as worker:
            self.assertEqual(chain.run(self.root), 0)
            enqueue.assert_not_called()
            worker.assert_not_called()
        saved = json.loads((self.root / 'state.json').read_text())
        self.assertTrue(saved['halted'])
        self.assertEqual(saved['jobs']['11']['resume']['state']['stop_reason'], 'NaN_or_Inf')

    def test_yield_waits_for_other_users(self):
        self.state['jobs']['11'] = dict(parent_job=10, world=8, wall_seconds=14400,
            dependency='afterany:10', state='PENDING', settled=False)
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
        self.assertFalse(any(args[0] == 'sbatch' for args in calls))


if __name__ == '__main__':
    unittest.main()
