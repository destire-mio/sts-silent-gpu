import sys
import threading
import unittest

import numpy as np
import spire


class TrainingTests(unittest.TestCase):
    def test_preallocated_thread_determinism_and_clone(self):
        seeds = list(range(1000000000, 1000001000))
        a = spire.TrainingBatch(8, 1, 20, seeds)
        b = spire.TrainingBatch(8, 4, 20, seeds)
        fields = ("ids", "values", "candidates", "entity_mask", "candidate_mask", "rewards", "done",
                  "truncated", "faults", "reset_mask", "advanced", "seed_indices", "decision_counts")
        buffers_a = [getattr(a, name) for name in fields]
        buffers_b = [getattr(b, name) for name in fields]
        addresses = [x.ctypes.data for x in buffers_a]
        selected = np.zeros(8, dtype=np.int32)
        for _ in range(30):
            a.step(selected)
            b.step(selected)
            for x, y in zip(buffers_a, buffers_b):
                np.testing.assert_array_equal(x, y)
        self.assertEqual(addresses, [getattr(a, name).ctypes.data for name in fields])
        a.clone(0, 1)
        for name in ("ids", "values", "candidates", "entity_mask", "candidate_mask"):
            np.testing.assert_array_equal(getattr(a, name)[0], getattr(a, name)[1])
        self.assertEqual(a.rewards[1], 0)
        self.assertFalse(a.ids.flags.writeable)

    def test_buffer_lifetime_and_input_contract(self):
        batch = spire.TrainingBatch(1, 1, 10, [1000000000, 1000000001])
        ids = batch.ids
        with self.assertRaises(TypeError):
            batch.step(np.zeros(1, dtype=np.int64))
        with self.assertRaises(ValueError):
            batch.step(np.zeros(2, dtype=np.int32))
        del batch
        self.assertEqual(ids[0, 0, 0], 1)

    def test_step_releases_gil(self):
        batch = spire.TrainingBatch(128, 4, 100, list(range(1000000000, 1000000256)))
        ready, start, ran = threading.Event(), threading.Event(), threading.Event()

        def python_thread():
            ready.set()
            start.wait()
            ran.set()

        worker = threading.Thread(target=python_thread)
        worker.start()
        ready.wait()
        interval = sys.getswitchinterval()
        try:
            sys.setswitchinterval(1)
            start.set()
            batch.step(np.zeros(128, dtype=np.int32))
            self.assertTrue(ran.is_set(), "Python thread could not run while C++ stepped")
        finally:
            sys.setswitchinterval(interval)
            worker.join()
