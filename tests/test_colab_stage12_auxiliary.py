import gzip
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest
import torch

from train.e4_stage12_auxiliary import GAE_LAMBDA, MILESTONES, lambda_targets, roc_auc


def test_auxiliary_lambda_targets_respect_boundaries():
    prediction = torch.tensor([.2, .4, .6, .8]).reshape(4, 1, 1)
    logits = prediction.logit().expand(-1, 4, -1)
    eligible = torch.tensor([[1, 1, 1, 1], [1, 1, 0, 1], [0, 1, 1, 0], [1, 1, 1, 1]]).bool().unsqueeze(-1)
    resolved = torch.zeros_like(eligible)
    resolved[1, 0] = resolved[0, 3] = True
    outcome = torch.zeros_like(logits)
    outcome[1, 0] = 1.
    endpoint = torch.full((4, 1), .9, requires_grad=True)
    targets, valid, observed = lambda_targets(
        dict(logits=logits, eligible=eligible, resolved=resolved, outcome=outcome),
        endpoint, torch.ones_like(endpoint, dtype=torch.bool))
    assert torch.equal(valid[:, 0, 0], torch.tensor([True, True, False, True]))
    assert torch.equal(observed[:, 0, 0], torch.tensor([True, True, False, False]))
    assert torch.equal(targets[:2, 0, 0], torch.ones(2))
    assert not bool(valid[0, 2, 0])
    assert targets[0, 3, 0] == 0.
    expected = .9
    for p in (.8, .6, .4):
        expected = GAE_LAMBDA * expected + (1 - GAE_LAMBDA) * p
    assert float(targets[0, 1, 0]) == pytest.approx(expected)
    assert not targets.requires_grad
    assert roc_auc(torch.tensor([.2, .4, .4, .7]), torch.tensor([0., 1., 0., 1.])) == .875


def test_dev132_auxiliary_update_and_restart(tmp_path):
    source = os.environ.get("E4_TEST_DEV132")
    if source is None:
        pytest.skip("set E4_TEST_DEV132 to the real 768-wide Stage12 checkpoint")
    assert Path(source).is_file()
    output = tmp_path / "training"
    output.mkdir()
    with gzip.open(output / "baseline.json.gz", "wt") as stream:
        stream.write("[]")
    env = dict(os.environ, CUDA_VISIBLE_DEVICES="", OMP_NUM_THREADS="1", MKL_NUM_THREADS="1")
    env.pop("SLURM_JOB_ID", None)
    for update in (1, 2):
        entry = tmp_path / "run_stage12.py"
        entry.write_text(f"from tools.e4_stage12 import main\nmain(device_type='cpu', stop_after_update={update})\n")
        args = [sys.executable, "-m", "torch.distributed.run", "--standalone", "--nproc_per_node=1",
                str(entry), "--output", str(output), "--lanes", "2", "--rollout-length", "32", "--microbatch", "16"]
        if update == 1:
            args += ["--init", source, "--combat-aux-weight", ".2", "--global-aux-weight", ".3",
                     "--value-coefficient", "76.96145098000254"]
        process = subprocess.run(args, env=env, text=True, capture_output=True, timeout=300)
        assert process.returncode == 0, process.stdout + process.stderr
        events = [json.loads(line) for line in process.stdout.splitlines() if line.startswith("{")]
        if update == 2:
            assert any(e.get("event") == "resumed" and e["update"] == 1 for e in events)
        row, = [e for e in events if e.get("event") == "update"]
        assert row["finite"] and row["optimizer_steps"] == 2
        auxiliary = row["auxiliary"]
        assert auxiliary["combat"]["labels"] > 0 and auxiliary["combat"]["hp_mse"] >= 0
        assert set(auxiliary["global"]["milestones"]) == set(MILESTONES)
        assert all(m["bce"] >= 0 for m in auxiliary["global"]["milestones"].values())
        saved = torch.load(output / "model.pt", map_location="cpu", weights_only=True, mmap=True)
        assert saved["state"]["update"] == update
        assert saved["state"]["standalone_auxiliary_weights"] == {"combat": .2, "global": .3}
        heads = {name for name in saved["model"] if name.startswith("auxiliary_heads.")}
        assert len(heads) == 8
        optimizer = saved["optimizer"]
        assert len(optimizer["param_groups"]) == 2
        for parameter in optimizer["param_groups"][1]["params"]:
            assert int(optimizer["state"][parameter]["step"]) == 2 * update
            assert torch.count_nonzero(optimizer["state"][parameter]["exp_avg"]) > 0
        del saved, optimizer
        print(json.dumps(dict(update=update, optimizer_steps=row["optimizer_steps"], auxiliary=auxiliary)))




def test_zero_weights_match_prior_learner(tmp_path):
    import importlib.util
    from copy import deepcopy

    import torch.distributed as dist
    from torch.nn.parallel import DistributedDataParallel

    from train.e4_stage12 import CLSPolicy, NoSLCollector, NoSLSearch, advance_minibatch, new_cursor
    from train.model import ModelConfig

    source = os.environ.get("E4_TEST_PRE_AUX_STAGE12")
    if source is None:
        pytest.skip("set E4_TEST_PRE_AUX_STAGE12 to train/e4_stage12.py from before auxiliary heads")
    spec = importlib.util.spec_from_file_location("stage12_reference", source)
    reference = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reference)

    def assert_bitwise(actual, expected):
        assert type(actual) is type(expected)
        if isinstance(actual, torch.Tensor):
            assert actual.dtype == expected.dtype and actual.shape == expected.shape
            assert torch.equal(actual.contiguous().reshape(-1).view(torch.uint8),
                               expected.contiguous().reshape(-1).view(torch.uint8))
        elif isinstance(actual, dict):
            assert actual.keys() == expected.keys()
            for key in actual:
                assert_bitwise(actual[key], expected[key])
        elif isinstance(actual, (tuple, list)):
            assert len(actual) == len(expected)
            for a, b in zip(actual, expected):
                assert_bitwise(a, b)
        else:
            assert actual == expected

    torch.manual_seed(1729)
    torch.set_num_threads(1)
    dist.init_process_group("gloo", init_method=(tmp_path / "gloo").as_uri(), rank=0, world_size=1)
    try:
        device = torch.device("cpu")
        policy = CLSPolicy(ModelConfig(384, 6, 6, 1536)).eval()
        collector = NoSLCollector(NoSLSearch(2, device, 11), torch.Generator().manual_seed(12))
        fragment = collector.collect(policy, length=8, auxiliary_weights={})
        assert "auxiliary" not in fragment
        outcomes = []
        for learner, options in [(reference.advance_minibatch, {}), (advance_minibatch, {"auxiliary_weights": {}})]:
            model = DistributedDataParallel(deepcopy(policy).train(), broadcast_buffers=False)
            optimizer = torch.optim.Adam(model.parameters(), lr=1e-4, eps=1e-5)
            rng = torch.Generator().manual_seed(13)
            cursor = new_cursor(deepcopy(fragment), rng, 1)
            learner(model, optimizer, torch.amp.GradScaler("cpu", enabled=False), cursor, rng, 0, 1,
                    microbatch=8, value_state={"standalone_value_coefficient": .5}, **options)
            assert cursor["optimizer_steps"] == 1
            outcomes.append((model.module.state_dict(), optimizer.state_dict(), cursor, rng.get_state()))
        assert_bitwise(*outcomes)
    finally:
        dist.destroy_process_group()
