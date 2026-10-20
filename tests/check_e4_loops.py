import argparse
import hashlib
import json
from pathlib import Path
import time

import torch

from train.e4 import FLOOR, SCREEN, RoomSearch
from train.model import SCREENS


def keys(batch, device):
    result = torch.empty(1, dtype=torch.int64, device=device)
    batch.loop_keys_tensors(torch.tensor([0], dtype=torch.int64, device=device), result)
    return int(result[0])


def check(witness, gpu_search):
    gpu = gpu_search.batch
    device = torch.device("cpu")
    search = RoomSearch(1, 8, device, torch.Generator().manual_seed(1729), scratch_slots=2)
    seed = witness["seed"]
    search.batch.reset_to(0, seed, [])
    search.provenance[0] = (seed, [])
    search.reset_seen([0])
    gpu.reset_to(0, seed, [])
    assert keys(gpu, torch.device("cuda", 0)) == search.loop_keys([0])[0]
    target = dict(neow="map", shop="shop", combat_reward="combat_reward")[witness["kind"]]
    prefix = []
    start = time.monotonic()
    blocked = None
    for index, action in enumerate(witness["candidates"]):
        before = search.progress()[0]
        search.clone([0], [1])
        search.batch.step_slots([0], [action])
        search.check([0])
        after = search.progress()[0]
        key = search.loop_keys([0])[0]
        gpu.step_slots([0], [action])
        assert not int(torch.as_tensor(gpu.faults, device="cuda").to(torch.int64)[0])
        assert keys(gpu, torch.device("cuda", 0)) == key, (witness["kind"], index)
        repeats = before[FLOOR] == after[FLOOR] and key in search.seen[0]
        if repeats and SCREENS[before[SCREEN]] == target:
            search.clone([0], [2])
            search.clone([1], [0])
            raw = search.batch.state_bytes(0)
            mask = search.policy_masks([0])[0]
            assert search.batch.state_bytes(0) == raw, "One-step probes mutated the source"
            if not bool(mask[action]):
                assert bool(mask.any())
                gpu.reset_to(0, seed, prefix)
                gpu_search.provenance[0] = (seed, prefix.copy())
                gpu_search.seen[0] = search.seen[0].copy()
                gpu_search.mask_cache.clear()
                gpu_mask = gpu_search.policy_masks([0])[0].cpu()
                assert torch.equal(mask, gpu_mask), "CPU/GPU repeat masks differ"
                blocked = dict(kind=witness["kind"], seed=seed, decision=index, candidate=action,
                               screen=target, floor=before[FLOOR], resulting_key=key,
                               cpu_gpu_equal_states=index + 2, repeat_mask_blocks=True,
                               cpu_gpu_masks_equal=True,
                               source_unchanged_by_probes=True, source=witness["source"],
                               source_sha256=witness["source_sha256"], prefix=prefix + [action],
                               loop_metrics=search.loop_metrics.copy())
                break
            search.clone([2], [0])
        if before[FLOOR] != after[FLOOR]:
            search.seen[0] = set()
        search.seen[0].add(key)
        search.mask_cache.pop(0, None)
        prefix.append(action)
        search.provenance[0] = (seed, prefix.copy())
    assert blocked is not None, f"Preserved {witness['kind']} loop was not blocked"
    blocked["replay_seconds"] = time.monotonic() - start
    return blocked


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("witnesses", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    torch.cuda.set_device(0)
    device = torch.device("cuda", 0)
    gpu = RoomSearch(1, 8, device, torch.Generator(device=device).manual_seed(1729), scratch_slots=2)
    witnesses = json.loads(args.witnesses.read_text())
    assert {w["kind"] for w in witnesses} == {"neow", "shop", "combat_reward"}
    result = dict(witness_sha256=hashlib.sha256(args.witnesses.read_bytes()).hexdigest(),
                  checks=[check(w, gpu) for w in witnesses],
                  scope="CPU legacy-path replay; repeat-only mask with map-return flag false; CPU/GPU key parity at every replayed state")
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
