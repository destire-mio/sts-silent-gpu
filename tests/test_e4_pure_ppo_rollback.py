import ast
from copy import deepcopy
import json
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import torch

ROOT = Path(__file__).resolve().parents[1]
TRAINER = ast.parse((ROOT/"tools/e4_stage12.py").read_text())
ROLLBACK = next(n for n in TRAINER.body if isinstance(n, ast.FunctionDef) and n.name == "rollback_dev78")
MAIN = next(n for n in TRAINER.body if isinstance(n, ast.FunctionDef) and n.name == "main")


def load_rollback():
    namespace = dict(torch=torch)
    exec(compile(ast.Module(body=[ROLLBACK], type_ignores=[]), "rollback", "exec"), namespace)
    return namespace["rollback_dev78"]


def test_rollback_keeps_adam_clock_calibration_rng_and_dev80(tmp_path):
    policy = torch.nn.Linear(2, 1)
    optimizer = torch.optim.Adam(policy.parameters(), lr=.01, eps=1e-5)
    policy(torch.ones(3, 2)).square().mean().backward()
    optimizer.step()
    optimizer.zero_grad()
    adam = deepcopy(optimizer.state_dict())
    old = deepcopy(policy.state_dict())
    torch.save(old, tmp_path/"dev-80.pt")
    dev80 = (tmp_path/"dev-80.pt").read_bytes()
    target = {k:torch.full_like(v, .25) for k,v in old.items()}
    torch.save(target, tmp_path/"dev-78.pt")
    state = dict(update=1540, training_seconds=259300., checkpoint_serial=170,
                 next_evaluation=295200., claude147_dev70_rollback=dict(update=1561),
                 claude148_expert_v2=dict(update=1688), value_calibration=dict(value_coefficient=181.391, completed_updates=1000))
    before = deepcopy(state)
    rng = torch.get_rng_state().clone()
    assert load_rollback()(policy, state, tmp_path)
    assert all(torch.equal(policy.state_dict()[k], v) for k,v in target.items())
    assert {k:v for k,v in state.items() if k!="claude150_dev78_rollback"} == before
    assert torch.equal(torch.get_rng_state(), rng)
    assert (tmp_path/"dev-80.pt").read_bytes() == dev80
    reference = torch.nn.Linear(2, 1)
    reference.load_state_dict(target)
    other = torch.optim.Adam(reference.parameters(), lr=.01, eps=1e-5)
    other.load_state_dict(adam)
    for model, opt in ((policy, optimizer), (reference, other)):
        model(torch.ones(3, 2)).square().mean().backward()
        opt.step()
    assert all(torch.equal(policy.state_dict()[k], v) for k,v in reference.state_dict().items())


def test_saved_marker_prevents_repeating_the_weight_rollback(tmp_path):
    policy = torch.nn.Linear(2, 1)
    state = dict(update=1540, training_seconds=259300.)
    torch.save(policy.state_dict(), tmp_path/"dev-78.pt")
    rollback = load_rollback()
    assert rollback(policy, state, tmp_path)
    with torch.no_grad():
        policy.weight.add_(1)
    torch.save(dict(model=policy.state_dict(), state=state), tmp_path/"resume.pt")
    saved = torch.load(tmp_path/"resume.pt", weights_only=True)
    restored = torch.nn.Linear(2, 1)
    restored.load_state_dict(saved["model"])
    with patch.object(torch, "load", side_effect=AssertionError("must not reload dev78")):
        assert not rollback(restored, saved["state"], tmp_path)
    assert all(torch.equal(restored.state_dict()[k], v) for k,v in saved["model"].items())


def test_first_rollback_discards_old_ppo_and_expert_cursor_preserving_pools(tmp_path):
    policy = torch.nn.Linear(2, 1)
    torch.save(policy.state_dict(), tmp_path/"dev-78.pt")
    pools = [[{"seed":i}] for i in range(4)]
    collector = SimpleNamespace(partial={"old_policy":True}, pools=pools, rng=torch.Generator().manual_seed(12))
    rng = collector.rng.get_state().clone()
    scope = dict(rollback_dev78=load_rollback(), policy=policy, state=dict(update=1539, training_seconds=259242.),
                 output=tmp_path, cursor=dict(optimizer_steps=7, expert_total=100),
                 collector=collector, round_state=dict(ready={"old_fragment":True}, seconds=4.),
                 rank=0, world=3, json=json)
    gate = next(n for n in MAIN.body if isinstance(n,ast.If) and isinstance(n.test,ast.Call)
                and isinstance(n.test.func,ast.Name) and n.test.func.id=="rollback_dev78")
    exec(compile(ast.Module(body=[gate],type_ignores=[]), "restore", "exec"), scope)
    assert scope["cursor"] is None and scope["round_state"]["ready"] is None
    assert collector.partial is None and collector.pools is pools
    assert torch.equal(collector.rng.get_state(), rng)
    assert scope["retained_partial_steps"] == 7
    calls = [n for n in ast.walk(MAIN) if isinstance(n,ast.Call)]
    assert not any(isinstance(n.func,ast.Name) and n.func.id in ("ExpertIteration","attach_expert") for n in calls)


def test_controller_partial_bookkeeping_clears_only_on_the_first_rollback():
    tree = ast.parse((ROOT/"tools/e4_stage12_chain.py").read_text())
    run = next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=="run")
    gate = next(n for n in ast.walk(run) if isinstance(n,ast.If)
                and "claude150_dev78_rollback" in ast.unparse(n.test))
    for applied in (False, True):
        checkpoint = dict(state=dict(world=3, claude147_dev70_rollback=dict(update=1561),
            claude148_expert_v2=dict(update=1688)), partial_steps=7, partial_attempts=7, partial_skips=0)
        if applied:
            checkpoint["state"]["claude150_dev78_rollback"] = {"checkpoint":"dev-78.pt"}
            checkpoint["state"]["claude148_expert_v2"] = {"ce_gradient_cap":.25}
        scope = dict(checkpoint=checkpoint, state={"jobs":{"1":{"world":3}}}, identity="1")
        exec(compile(ast.Module(body=[gate],type_ignores=[]), "controller", "exec"), scope)
        assert checkpoint["partial_steps"] == (7 if applied else 0)
        if not applied:
            assert checkpoint["retained_partial_optimizer_steps"] == 7
