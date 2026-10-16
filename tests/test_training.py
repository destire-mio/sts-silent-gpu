import sys
from pathlib import Path
import threading
import unittest

import numpy as np
import spire
import training_state


class TrainingTests(unittest.TestCase):
    def test_preallocated_thread_determinism_and_clone(self):
        seeds = list(range(1000000000, 1000001000))
        a = spire.TrainingBatch(8, 1, 20, seeds)
        b = spire.TrainingBatch(8, 4, 20, seeds)
        fields = ("tags", "ids", "values", "candidate_kinds", "candidate_refs", "entity_mask", "candidate_mask", "rewards", "done",
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
        for name in ("tags", "ids", "values", "candidate_kinds", "candidate_refs", "entity_mask", "candidate_mask"):
            np.testing.assert_array_equal(getattr(a, name)[0], getattr(a, name)[1])
        self.assertEqual(a.rewards[1], 0)
        self.assertFalse(a.ids.flags.writeable)
        self.assertEqual(a.tags.dtype, np.uint8)
        self.assertEqual(a.ids.dtype, np.int16)
        self.assertEqual(a.values.dtype, np.int16)
        self.assertEqual(a.candidate_kinds.dtype, np.uint8)
        self.assertEqual(a.candidate_refs.dtype, np.int16)
        self.assertEqual(a.entity_mask.shape, (8, 48))
        self.assertEqual(a.candidate_mask.shape, (8, 16))
        self.assertEqual(spire.observation_bytes, 10184)

    def test_buffer_lifetime_and_input_contract(self):
        batch = spire.TrainingBatch(1, 1, 10, [1000000000, 1000000001])
        ids = batch.tags
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

    def test_compact_append_gather_and_capacity(self):
        batch = spire.TrainingBatch(4, 2, 30, list(range(1000000000, 1000000100)))
        slots = np.arange(4, dtype=np.int32)
        data = np.full(20000, 165, dtype=np.uint8)
        offsets = np.full(13, 999, dtype=np.uint64)
        offsets[0] = 3
        fields = ("tags", "ids", "values", "candidate_kinds", "candidate_refs", "entity_mask", "candidate_mask",
                  "entity_counts", "candidate_counts")
        expected = {name: [] for name in fields}
        for turn in range(3):
            for name in fields:
                expected[name].extend(getattr(batch, name).copy())
            start = turn * 4
            self.assertEqual(batch.store(slots, data, offsets[start:start + 5]), 0)
            expected_sizes = 4 + 24 * batch.entity_counts + 7 * batch.candidate_counts
            np.testing.assert_array_equal(np.diff(offsets[start:start + 5]), expected_sizes)
            batch.step(np.zeros(4, dtype=np.int32))
        self.assertTrue((data[:3] == 165).all())
        self.assertTrue((data[int(offsets[-1]):] == 165).all())
        minibatch = spire.ObservationBatch(5)
        views = [getattr(minibatch, name) for name in fields]
        addresses = [view.ctypes.data for view in views]
        for indices in (np.array([7, 0, 11, 7, 1], dtype=np.int32), np.array([0, 0, 0, 0, 0], dtype=np.int32)):
            minibatch.pad(data, offsets, indices)
            for name, view in zip(fields, views):
                np.testing.assert_array_equal(view, np.array(expected[name])[indices])
        self.assertEqual(addresses, [getattr(minibatch, name).ctypes.data for name in fields])
        self.assertFalse(minibatch.tags.flags.writeable)
        too_small = np.full(1, 165, dtype=np.uint8)
        output_offsets = np.full(5, 999, dtype=np.uint64)
        output_offsets[0] = 0
        self.assertEqual(batch.store(slots, too_small, output_offsets), 0xe0000007)
        np.testing.assert_array_equal(output_offsets, [0, 999, 999, 999, 999])
        np.testing.assert_array_equal(too_small, [165])
        self.assertFalse(batch.faults.any())
        output_offsets[0] = np.iinfo(np.uint64).max
        self.assertEqual(batch.store(slots, data, output_offsets), 0xe0000007)
        self.assertEqual(output_offsets[0], np.iinfo(np.uint64).max)
        output_offsets[0] = 0
        self.assertEqual(batch.store(slots[:0], too_small, output_offsets[:1]), 0)
        with self.assertRaises(TypeError):
            batch.store(slots.astype(np.int64), data, offsets[:5])
        with self.assertRaises(ValueError):
            batch.store(slots, data, offsets[:4])
        with self.assertRaises(ValueError):
            minibatch.pad(data, offsets, np.array([0, 1, 2, 3, 12], dtype=np.int32))

    def test_deep_prefix_game_snapshots(self):
        words = iter(map(int, (Path(__file__).resolve().parent / "deep_benchmark.actions").read_text().split()))
        names = ("play", "end_turn", "choose", "potion_use", "potion_discard", "proceed", "skip")
        for seed in words:
            count = next(words)
            raw = [(next(words), next(words), next(words)) for _ in range(count)]
            if seed != 202610180018:
                continue
            game = spire.reset(seed)
            prefix = []
            for kind, index, target in raw:
                action = dict(type=names[kind])
                if kind == 0:
                    action.update(card=index, target=target)
                elif kind == 2:
                    action.update(option=index)
                elif kind in (3, 4):
                    action.update(slot=index)
                    if kind == 3:
                        action.update(target=target)
                prefix.append(game.legal_actions().index(action))
                game.step(action)
            for distance in (8, 32, 128):
                live, replayed = training_state.prefix_states(seed, prefix[:-distance])
                self.assertEqual(live, replayed)
            break
        else:
            self.fail("Missing accepted winning route")
