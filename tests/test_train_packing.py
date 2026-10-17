import unittest

import numpy as np
import torch

from train.model import FIELDS, ModelConfig, Policy, length_batches, network_inputs, packed_inputs, record_lengths
from train.ppo import PPOConfig, objective


def fixed_observations():
    rng = np.random.default_rng(431)
    entities = np.array([17, 67, 147, 96, 33])
    candidates = np.array([1, 4, 31, 2, 9])
    arrays = dict(tags=np.zeros((5, 4, 384), dtype=np.uint8), ids=np.zeros((5, 4, 384), dtype=np.int16),
                  values=np.zeros((5, 6, 384), dtype=np.int16), candidate_kinds=np.zeros((5, 128), dtype=np.uint8),
                  candidate_refs=np.zeros((5, 3, 128), dtype=np.int16))
    records, offsets = [], [0]
    for i, (e, c) in enumerate(zip(entities, candidates)):
        arrays["tags"][i, :, :e] = rng.integers(1, 12, (4, e))
        arrays["ids"][i, :, :e] = rng.integers(1, 385, (4, e))
        arrays["values"][i, :, :e] = rng.integers(-20, 100, (6, e))
        arrays["candidate_kinds"][i, :c] = rng.integers(1, 8, c)
        arrays["candidate_refs"][i, :, :c] = rng.integers(0, e + 1, (3, c))
        arrays["candidate_refs"][i, :, c - 1] = [0, e, 1]
        record = np.array([e, c], dtype="<u2").tobytes()
        for name in FIELDS[:5]:
            width = c if name.startswith("candidate") else e
            record += arrays[name][i, ..., :width].tobytes()
        records.append(record)
        offsets.append(offsets[-1] + len(record))
    arrays["entity_mask"] = np.packbits(np.arange(384) < entities[:, None], axis=-1, bitorder="little")
    arrays["candidate_mask"] = np.packbits(np.arange(128) < candidates[:, None], axis=-1, bitorder="little")
    return arrays, np.frombuffer(b"".join(records), dtype=np.uint8), np.array(offsets, dtype=np.uint64)


def capacity_inputs(arrays):
    result = {name: torch.tensor(arrays[name], dtype=torch.float32 if name == "values" else torch.long)
              for name in FIELDS[:5]}
    for name in FIELDS[5:]:
        result[name] = torch.tensor(np.unpackbits(arrays[name], axis=-1, bitorder="little").astype(bool))
    return result


class PackingTests(unittest.TestCase):
    def test_capacity_logits_losses_and_every_parameter_gradient(self):
        torch.set_num_threads(1)
        torch.manual_seed(77)
        arrays, data, offsets = fixed_observations()
        model = Policy(ModelConfig(width=32, layers=2, heads=4, feedforward=64))
        packed = packed_inputs(data, offsets, np.arange(5), torch.device("cpu"))
        cropped = network_inputs(arrays, torch.device("cpu"), np.arange(5))
        for name in FIELDS:
            torch.testing.assert_close(packed[name], cropped[name], atol=0, rtol=0)
        self.assertEqual(packed["tags"].shape[-1], 147)
        self.assertEqual(packed["candidate_kinds"].shape[-1], 31)
        actions = torch.tensor([0, 3, 30, 1, 8])
        old = torch.tensor([0., -1.1, -3.7, -0.6, -2.5])
        returns = torch.tensor([0., 1., 0.4, 1., 0.])
        advantages = torch.tensor([-1., 0.8, -0.2, 1.2, -0.8])
        results = []
        for inputs in (capacity_inputs(arrays), packed):
            logits, values = model(inputs)
            loss, metrics = objective(logits, values, inputs["candidate_mask"], actions, old, returns,
                                      advantages, PPOConfig())
            model.zero_grad(set_to_none=True)
            loss.backward()
            results.append((logits.detach()[:, :31], values.detach(), loss.detach(),
                            {k: v.detach() for k, v in metrics.items()},
                            {k: p.grad.detach().clone() for k, p in model.named_parameters()}))
        max_error = 0.
        for first, second in zip(*results):
            pairs = [(first, second)] if isinstance(first, torch.Tensor) else [(first[k], second[k]) for k in first]
            for a, b in pairs:
                torch.testing.assert_close(a, b, atol=3e-6, rtol=3e-5)
                finite = torch.isfinite(a)
                max_error = max(max_error, float((a[finite] - b[finite]).abs().max()))
        model.eval()
        with torch.no_grad():
            full_logits, full_values = model(capacity_inputs(arrays))
            logits, values = model(packed)
        torch.testing.assert_close(logits, full_logits[:, :31], atol=3e-6, rtol=3e-5)
        torch.testing.assert_close(values, full_values, atol=3e-6, rtol=3e-5)
        print(f"packing logits/loss/gradients max absolute error: {max_error:.3g}")

    def test_selected_records_and_length_batches_preserve_samples(self):
        arrays, data, offsets = fixed_observations()
        selected = np.array([4, 0, 4])
        packed = packed_inputs(data, offsets, selected, torch.device("cpu"))
        expected = network_inputs(arrays, torch.device("cpu"), selected)
        for name in FIELDS:
            torch.testing.assert_close(packed[name], expected[name], atol=0, rtol=0)
        entities, candidates = record_lengths(data, offsets, np.arange(5))
        batches = length_batches(np.arange(5), entities, candidates, 2, np.random.default_rng(9))
        np.testing.assert_array_equal(np.sort(np.concatenate(batches)), np.arange(5))
        self.assertEqual(sorted(map(len, batches)), [1, 2, 2])


if __name__ == "__main__":
    unittest.main()
