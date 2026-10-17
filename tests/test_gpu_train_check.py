from contextlib import redirect_stdout
import io
import math
import os
from pathlib import Path
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest

import torch

from test_train_packing import fixed_observations
from tools.gpu_train_check import (CAP, MIB, cycled_roots, observation_profile, rates,
                                   run_stage, scaling_table, select_probe, storage_mib)


class GPUCheckTests(unittest.TestCase):
    def test_storage_uses_measured_compact_lengths(self):
        _, data, offsets = fixed_observations()
        rollout = SimpleNamespace(data=torch.tensor(data), offsets=torch.tensor(offsets.astype('int64')), count=5)
        profile = observation_profile(rollout)
        maximum = 4 + 24 * 147 + 7 * 31
        self.assertEqual(profile['record_bytes']['max'], maximum)
        self.assertEqual(profile['entities']['max'], 147)
        self.assertEqual(profile['candidates']['max'], 31)
        self.assertEqual(profile['storage_bytes_per_decision'], math.ceil(maximum * 1.25))
        self.assertEqual(storage_mib(8192, profile), math.ceil(8192 * CAP * math.ceil(maximum * 1.25) / MIB))
        self.assertLess(storage_mib(8192, profile), 8192 * CAP * 10180 / MIB)

    def test_weak_scaling_repeats_root_mixture_per_gpu(self):
        roots = [dict(seed=i, candidates=[]) for i in range(3)]
        expected = cycled_roots(roots, 7)
        for world in (1, 2, 8):
            group = cycled_roots(roots, 7, world)
            self.assertEqual(len(group), 7 * world)
            for rank in range(world):
                self.assertEqual(group[rank::world], expected)

    def test_sweep_top_is_a_fit_and_selection_uses_end_to_end_rate(self):
        rows = [dict(status='fit', lanes_per_gpu=512, inference_chunk=512,
                     collect_decisions_per_second=1000, total_decisions_per_second=100),
                dict(status='fit', lanes_per_gpu=32768, inference_chunk=2048,
                     collect_decisions_per_second=800, total_decisions_per_second=200)]
        self.assertIs(select_probe(rows), rows[-1])
        self.assertIs(select_probe(rows + [dict(status='timeout'), dict(status='oom')]), rows[-1])

    def test_weak_efficiency_uses_per_gpu_rate(self):
        reports = [dict(workers=world, updates=[], **rates(decisions, collect, total, world))
                   for world, decisions, collect, total in ((1, 100, 1, 2), (2, 180, 1.5, 3), (8, 800, 4, 5))]
        table = scaling_table(reports)
        self.assertEqual([r['total_decisions_per_second'] for r in table], [50, 60, 160])
        self.assertEqual([r['per_gpu_decisions_per_second'] for r in table], [50, 30, 20])
        self.assertEqual([r['efficiency'] for r in table], [1, .6, .4])
        self.assertEqual([r['collect_decisions_per_second'] for r in table], [100, 120, 200])

    def test_timeout_kills_worker_process_group_and_preserves_log(self):
        with tempfile.TemporaryDirectory() as directory, redirect_stdout(io.StringIO()):
            path = Path(directory)
            child = 'import pathlib,time; time.sleep(1); pathlib.Path("leaked").touch()'
            parent = f'import subprocess,sys,time; subprocess.Popen([sys.executable,"-c",{child!r}],cwd={directory!r}); print("started",flush=True); time.sleep(30)'
            before = time.monotonic()
            result = run_stage('bounded', [sys.executable, '-c', parent], path, before + .4, 30, os.environ.copy())
            self.assertEqual(result['status'], 'timeout')
            self.assertLess(time.monotonic() - before, 3)
            self.assertIn('started', (path / 'bounded.log').read_text())
            time.sleep(1)
            self.assertFalse((path / 'leaked').exists())
            result = run_stage('success', [sys.executable, '-c', 'print(42)'], path,
                               time.monotonic() + 10, 10, os.environ.copy())
            self.assertEqual(result['status'], 'ok')
            self.assertEqual((path / 'success.log').read_text().strip(), '42')


if __name__ == '__main__':
    unittest.main()
