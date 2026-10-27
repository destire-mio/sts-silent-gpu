import random
import unittest
from unittest.mock import patch

import torch

from train.e4_timing import LabelTimer


class LabelTimerTest(unittest.TestCase):
    def test_disabled_preserves_return_rng_and_never_touches_cuda_or_clock(self):
        timer = LabelTimer()
        @timer.measure('learning')
        def operation(value):
            return value + torch.rand(3), random.random()
        torch.manual_seed(123)
        random.seed(123)
        expected = operation(torch.ones(3))
        expected_rng = torch.get_rng_state()
        expected_python = random.getstate()
        torch.manual_seed(123)
        random.seed(123)
        with patch('train.e4_timing.torch.cuda.synchronize', side_effect=AssertionError('disabled CUDA')), \
             patch('train.e4_timing.time.perf_counter', side_effect=AssertionError('disabled clock')):
            actual = operation(torch.ones(3))
        self.assertTrue(torch.equal(actual[0], expected[0]))
        self.assertEqual(actual[1], expected[1])
        self.assertTrue(torch.equal(torch.get_rng_state(), expected_rng))
        self.assertEqual(random.getstate(), expected_python)
        self.assertEqual(sum(timer.counts.values()), 0)

    def test_enabled_synchronizes_at_each_boundary_and_accumulates(self):
        timer = LabelTimer()
        timer.enabled = True
        order = []
        @timer.measure('learning')
        def operation(value):
            order.append('body')
            return value
        with patch('train.e4_timing.torch.cuda.synchronize', side_effect=lambda: order.append('sync')) as sync, \
             patch('train.e4_timing.time.perf_counter', side_effect=[10., 12., 20., 23.]):
            self.assertEqual(operation('first'), 'first')
            self.assertEqual(operation('second'), 'second')
        self.assertEqual(sync.call_count, 4)
        self.assertEqual(order, ['sync', 'body', 'sync'] * 2)
        report = timer.report(10., True)
        self.assertEqual(report['labels']['learning']['seconds'], 5.)
        self.assertEqual(report['labels']['learning']['count'], 2)
        self.assertEqual(report['label_seconds_sum'], 5.)
        self.assertEqual(report['labels']['learning']['fraction_of_label_sum'], 1.)
        self.assertEqual(report['labels']['learning']['fraction_of_update_wall'], .5)

    def test_nested_labels_are_explicitly_inclusive(self):
        timer = LabelTimer()
        timer.enabled = True
        @timer.measure('combat_sl')
        def inner():
            return 7
        @timer.measure('mainline')
        def outer():
            return inner()
        with patch('train.e4_timing.torch.cuda.synchronize') as sync, \
             patch('train.e4_timing.time.perf_counter', side_effect=[10., 11., 13., 15.]):
            self.assertEqual(outer(), 7)
        report = timer.report(6., True)
        self.assertEqual(sync.call_count, 4)
        self.assertEqual(report['labels']['combat_sl']['seconds'], 2.)
        self.assertEqual(report['labels']['mainline']['seconds'], 5.)
        self.assertEqual(report['label_seconds_sum'], 7.)
        self.assertAlmostEqual(sum(v['fraction_of_label_sum'] for v in report['labels'].values()), 1.)
        self.assertIn('overlap', report['scope'])

    def test_original_exception_propagates_after_exit_measurement(self):
        timer = LabelTimer()
        timer.enabled = True
        error = RuntimeError('original')
        @timer.measure('event_sl')
        def operation():
            raise error
        with patch('train.e4_timing.torch.cuda.synchronize') as sync, \
             patch('train.e4_timing.time.perf_counter', side_effect=[1., 4.]):
            with self.assertRaises(RuntimeError) as caught:
                operation()
        self.assertIs(caught.exception, error)
        self.assertEqual(sync.call_count, 2)
        self.assertEqual(timer.counts['event_sl'], 1)
        self.assertEqual(timer.seconds['event_sl'], 3.)

    def test_disabling_after_timing_stops_measurement(self):
        timer = LabelTimer()
        @timer.measure('repeat_check')
        def operation():
            return 8
        timer.enabled = True
        with patch('train.e4_timing.torch.cuda.synchronize'), \
             patch('train.e4_timing.time.perf_counter', side_effect=[1., 2.]):
            operation()
        timer.enabled = False
        with patch('train.e4_timing.torch.cuda.synchronize', side_effect=AssertionError('disabled')):
            self.assertEqual(operation(), 8)
        self.assertEqual(timer.seconds['repeat_check'], 1.)
        self.assertEqual(timer.counts['repeat_check'], 1)


if __name__ == '__main__':
    unittest.main()
