import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time

import spire

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tests"))
from replay import Difference, GAME_SHA256, compare, compare_scope, terminal_reason


class Lockstep:
    def __init__(self, seed, prefix, select):
        self.seed, self.prefix, self.select = seed, prefix, select
        self.game = spire.reset(seed)
        self.decisions = 0
        self.pending = None

    def decision(self, request):
        compare(self.seed, request["seed"], "$.seed")
        compare(self.decisions, request["decision"], "$.decision")
        compare(self.pending, request["previous"], "$.previous")
        if self.pending is not None:
            self.game.step(self.pending["action"])
            self.pending = None
        snapshot = self.game.snapshot()
        identities = ({}, {}, {}, {})
        for scope in ("combat", "run"):
            compare_scope(request["snapshot"], snapshot, scope, identities=identities)
        terminal = terminal_reason(snapshot, False)
        compare(terminal, request["terminal"], "$.terminal")
        legal = [] if terminal else self.game.legal_actions()
        compare(request["legal"], legal, "$.legal")
        if terminal:
            return dict(stop=terminal, failed=False)
        observation = self.game.observation()
        assert observation[-1] == 0, f"Observation fault: {observation[-1]}"
        replaying = self.decisions < len(self.prefix)
        index = self.prefix[self.decisions] if replaying else self.select(observation)
        if type(index) is not int or not 0 <= index < len(legal):
            raise Difference(f"$.index: controller returned {index!r} for {len(legal)} actions")
        self.pending = dict(index=index, action=legal[index])
        self.decisions += 1
        return dict(index=index, policy=not replaying)


def checkpoint_policy(path, sampling_seed):
    import numpy as np
    import torch
    from train.model import FIELDS, ModelConfig, Policy, distribution, network_inputs
    from train.rollout import sample

    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    checkpoint = torch.load(path, map_location="cpu", weights_only=True)
    assert checkpoint["config"]["observation_version"] == 2
    assert checkpoint["config"]["memory"] == "none"
    model = Policy(ModelConfig(**checkpoint["config"]["model"]))
    model.load_state_dict(checkpoint["model"])
    model.eval()
    generator = np.random.default_rng(sampling_seed)

    @torch.no_grad()
    def select(observation):
        arrays = {name: row[None] for name, row in zip(FIELDS, observation[:-1])}
        inputs = network_inputs(arrays, torch.device("cpu"), np.array([0]))
        logits, _ = model(inputs)
        policy, _ = distribution(logits, inputs["candidate_mask"])
        return int(sample(policy, [generator])[0])

    return select


def audit(instance):
    lines = (instance / "bytecode-audit.txt").read_text().splitlines()
    assert lines[0] == "loaded 2306 original 2306", lines[0]
    changed = {line.split()[0].removeprefix("com.megacrit.cardcrawl.") for line in lines[1:]}
    expected = json.loads((ROOT / "inventory/runtime-patches.json").read_text())
    assert changed == expected.keys(), (changed - expected.keys(), expected.keys() - changed)
    return len(changed)


def run(output, mod, root, select, headless, delay, metadata):
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    instance = output / "instance"
    instance.mkdir()
    oracle = ROOT / "oracle/instance"
    for name in ("desktop-1.0.jar", "ModTheSpire.jar", "launcher.jar", "jre"):
        (instance / name).symlink_to(oracle / name)
    for name in ("home", "tmp", "mods"):
        (instance / name).mkdir()
    (instance / "mods/Watch.jar").symlink_to(mod.resolve())
    state = Lockstep(root["seed"], root["candidates"], select)
    (output / "input.json").write_text(json.dumps(dict(root=root, **metadata), indent=2) + "\n")
    command = [str(instance / "jre/bin/java"), "-Xmx1G", "-XX:-UsePerfData",
               "-Djava.awt.headless=" + str(headless).lower(),
               "-Duser.home=" + str(instance / "home"), "-Djava.io.tmpdir=" + str(instance / "tmp"),
               "-Dwatch.config=" + str(instance / "watch.json"), "-javaagent:launcher.jar",
               "-cp", "launcher.jar:ModTheSpire.jar", "com.evacipated.cardcrawl.modthespire.Loader",
               "--mods", "oracle", "--skip-intro"]
    start = time.monotonic()
    with socket.socket() as server, (output / "game.log").open("w") as log:
        server.bind(("127.0.0.1", 0))
        server.listen(1)
        server.settimeout(60)
        config = dict(port=server.getsockname()[1], headless=headless, delay_ms=int(delay * 1000),
                      job=dict(seed=root["seed"], scenario=None))
        (instance / "watch.json").write_text(json.dumps(config))
        environment = dict(os.environ, HOME=str(instance / "home"), TMPDIR=str(instance / "tmp"))
        process = subprocess.Popen(command, cwd=instance, env=environment, stdout=log, stderr=subprocess.STDOUT)
        try:
            connection, _ = server.accept()
            with connection, connection.makefile("rw", encoding="utf-8") as channel, gzip.open(output / "trace.jsonl.gz", "wt") as trace:
                header = dict(schema=2, game_sha256=GAME_SHA256, seed=root["seed"], character="THE_SILENT",
                              ascension=20, scenario=None, playtime_at_least_800=False)
                trace.write(json.dumps(header) + "\n")
                while True:
                    line = channel.readline()
                    if not line:
                        raise RuntimeError(f"Game disconnected at decision {state.decisions}; see {output / 'game.log'}")
                    request = json.loads(line)
                    try:
                        response = state.decision(request)
                    except Difference as difference:
                        response = dict(stop=str(difference), failed=True)
                        (output / "difference.json").write_text(json.dumps(dict(request=request, engine=state.game.snapshot()), indent=2) + "\n")
                    channel.write(json.dumps(response) + "\n")
                    channel.flush()
                    if "stop" in response:
                        result = dict(seed=root["seed"], decisions=request["decision"],
                                      prefix_decisions=len(root["candidates"]),
                                      policy_decisions=max(0, request["decision"] - len(root["candidates"])),
                                      outcome="divergence" if response["failed"] else response["stop"],
                                      difference=response["stop"] if response["failed"] else None,
                                      seconds=time.monotonic() - start)
                        if not response["failed"]:
                            trace.write(json.dumps(dict(snapshot=request["snapshot"], terminal=request["terminal"])) + "\n")
                        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
                        print(json.dumps(result), flush=True)
                        break
                    trace.write(json.dumps(dict(snapshot=request["snapshot"], legal=request["legal"], action=state.pending["action"])) + "\n")
                    if request["decision"] % 100 == 0 or response["policy"]:
                        print(f"decision={request['decision']} floor={request['snapshot']['run']['floorNum']} "
                              f"{'policy' if response['policy'] else 'prefix'} index={response['index']}", flush=True)
            code = process.wait()
            result["patched_game_classes"] = audit(instance)
            result["exit_code"] = code
            (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
            assert code == (2 if result["outcome"] == "divergence" else 0), code
            return result
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait()


def read_root(path, index):
    if path.name.endswith(".jsonl.gz"):
        with gzip.open(path, "rt") as stream:
            return [json.loads(line) for line in stream][index]["root"]
    document = json.loads(path.read_text())
    assert document["observation_version"] == 2
    return document["roots"][index]


def main():
    parser = argparse.ArgumentParser(description="Watch a Silent A20 checkpoint through strict original-game lockstep")
    start = parser.add_mutually_exclusive_group(required=True)
    start.add_argument("--seed", type=int)
    start.add_argument("--roots", type=Path)
    parser.add_argument("--root-index", type=int, default=0)
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--sampling-seed", type=int, default=2718)
    parser.add_argument("--delay", type=float, default=0.5, help="Seconds before each policy action; prefix replay is accelerated")
    parser.add_argument("--mod", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="Fresh isolated instance and evidence directory")
    args = parser.parse_args()
    assert args.delay >= 0
    root = read_root(args.roots, args.root_index) if args.roots else dict(seed=args.seed, candidates=[])
    select = checkpoint_policy(args.checkpoint, args.sampling_seed)
    metadata = dict(checkpoint=str(args.checkpoint.resolve()), checkpoint_sha256=hashlib.sha256(args.checkpoint.read_bytes()).hexdigest(),
                    sampling_seed=args.sampling_seed, sampling="PCG64(sampling_seed), inverse CDF; independent of game seed",
                    engine_commit=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip())
    result = run(args.output, args.mod, root, select, False, args.delay, metadata)
    raise SystemExit(1 if result["outcome"] == "divergence" else 0)


if __name__ == "__main__":
    main()
