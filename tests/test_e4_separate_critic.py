import json
import os
import subprocess
import sys

import torch

from train.e4_stage12 import CLSPolicy, SeparateCritic
from train.model import ModelConfig


def test_separate_critic_starts_exact_and_routes_value_gradients():
    torch.manual_seed(1729)
    policy = CLSPolicy(ModelConfig(384, 6, 6, 1536)).eval()
    critic = SeparateCritic(policy).eval()
    inputs = dict(tags=torch.zeros((2, 4, 4), dtype=torch.long),
                  ids=torch.zeros((2, 4, 4), dtype=torch.long),
                  values=torch.arange(48).reshape(2, 6, 4).float(),
                  entity_mask=torch.ones((2, 4), dtype=torch.bool),
                  candidate_refs=torch.zeros((2, 3, 2), dtype=torch.long),
                  candidate_kinds=torch.ones((2, 2), dtype=torch.long),
                  candidate_mask=torch.ones((2, 2), dtype=torch.bool))
    with torch.no_grad():
        _, expected = policy(inputs)
        for combat in (torch.tensor([False, True]), torch.tensor([True, False])):
            assert torch.equal(critic(inputs, combat), expected)
    assert critic.cls.data_ptr() != policy.cls.data_ptr()
    assert critic.combat_value[0].weight.data_ptr() != critic.noncombat_value[0].weight.data_ptr()
    critic(inputs, torch.tensor([False, False])).sum().backward()
    assert all(parameter.grad is None for parameter in policy.parameters())
    assert torch.count_nonzero(critic.combat_value[0].weight.grad) == 0
    assert torch.count_nonzero(critic.noncombat_value[0].weight.grad) > 0


def test_separate_critic_cpu_updates_checkpoints_resumes_and_exams(tmp_path):
    init = tmp_path / "init.pt"
    torch.manual_seed(1729)
    weights = CLSPolicy(ModelConfig(384, 6, 6, 1536)).state_dict()
    torch.save(weights, init)
    output = tmp_path / "training"
    entry = tmp_path / "run.py"
    entry.write_text(
        """import sys

from tools import e4_stage12 as app

stop = int(sys.argv.pop(1))
base_search = app.NoSLSearch
base_evaluate = app.EvaluationLedgerCollector.evaluate


class SmallExamSearch(base_search):
    def __init__(self, lanes, *args, **kwargs):
        super().__init__(1 if kwargs.get("greedy") else lanes, *args, **kwargs)


def small_exam(self, policy, seeds):
    assert self.search.training_critic is None
    return base_evaluate(self, policy, seeds[:1])


app.NoSLSearch = SmallExamSearch
app.EvaluationLedgerCollector.evaluate = small_exam
app.main(device_type="cpu", stop_after_update=stop)
""",
        encoding="utf-8",
    )
    env = dict(os.environ, CUDA_VISIBLE_DEVICES="", OMP_NUM_THREADS="1", MKL_NUM_THREADS="1")
    env.pop("SLURM_JOB_ID", None)
    for update in (1, 2):
        args = [sys.executable, "-m", "torch.distributed.run", "--standalone", "--nproc_per_node", "1",
                str(entry), str(update), "--output", str(output), "--lanes", "2",
                "--rollout-length", "8", "--microbatch", "16"]
        if update == 1:
            args += ["--init", str(init), "--separate-critic", "--ema-decay", ".998"]
        run = subprocess.run(args, env=env, capture_output=True, text=True, timeout=110)
        assert run.returncode == 0, run.stdout + run.stderr
        events = [json.loads(line) for line in run.stdout.splitlines() if line.startswith("{")]
        effective, = [row for row in events if row.get("event") == "effective_config"]
        assert effective["critic"]["adam"] == ("fresh" if update == 1 else "restored")
        assert effective["critic"]["learning_rate"] == effective["learning_rate"]
        trained, = [row for row in events if row.get("event") == "update"]
        assert trained["update"] == update and trained["finite"] and trained["critic_optimizer_steps"] == 2
        saved = torch.load(output / "model.pt", map_location="cpu", weights_only=True)
        assert saved["state"]["update"] == update
        for optimizer in (saved["optimizer"], saved["critic"]["optimizer"]):
            assert {int(entry["step"]) for entry in optimizer["state"].values()} == {2 * update}
        for name in weights:
            if name.startswith("value."):
                assert torch.equal(saved["model"][name], weights[name])
        split = saved["critic"]
        assert not torch.equal(split["model"]["cls"], weights["cls"])
        assert not torch.equal(split["model"]["cls"], split["ema_weights"]["cls"])
        assert split["learning_rate_override"] is None
        del saved, split
    for tag, policy_name, critic_name in (
            ("evaluation", "dev-0.pt", "dev-0-critic.pt"),
            ("evaluation-ema", "dev-0-ema.pt", "dev-0-critic-ema.pt")):
        exam = json.loads((output / f"{tag}-0.json").read_text())
        assert exam["episodes"] == 1 and exam["complete"] and exam["no_SL"]
        assert exam["model_snapshot"] == policy_name and exam["critic_snapshot"] == critic_name
        assert (output / policy_name).is_file() and (output / critic_name).is_file()
