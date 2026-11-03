"""Local paired combat probe for a frozen Policy checkpoint; no training or search."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import subprocess
import time

import numpy as np
import torch

from train.model import FIELDS, ModelConfig, Policy, network_inputs


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_policy(path):
    checkpoint = torch.load(path, map_location="cpu", weights_only=True)
    model = Policy(ModelConfig(**checkpoint["config"]["model"])).eval()
    model.load_state_dict(checkpoint["model"], strict=True)
    return model


def remaining_potions(game):
    state = game.snapshot()
    player = state["objects"][state["player"]["ref"]]["fields"]
    potions = [state["objects"][p["ref"]]["fields"] for p in player["AbstractPlayer.potions"]]
    return sum(p["AbstractPotion.ID"] != "Potion Slot" for p in potions)


@torch.inference_mode()
def run(model, cases, cap, trace_limit):
    import spire

    results, observations = [], []
    for begin in range(0, len(cases), 16):
        group = cases[begin:begin + 16]
        games = [spire.start_combat(c["seed"], c["scenario"]) for c in group]
        rows = [dict(case=c["id"], seed=c["seed"], steps=0, actions=[],
                     action_counts=Counter(), ended_with_play_available=0) for c in group]
        live = list(range(len(games)))
        while live:
            raw = [games[i].observation() for i in live]
            assert all(r[-1] == 0 for r in raw)
            arrays = {name: np.stack([r[j] for r in raw]) for j, name in enumerate(FIELDS)}
            inputs = network_inputs(arrays, torch.device("cpu"), list(range(len(live))))
            logits, values = model(inputs)
            assert torch.isfinite(logits[inputs["candidate_mask"]]).all() and torch.isfinite(values).all()
            if len(observations) < trace_limit:
                observations.append({k: v.clone() for k, v in inputs.items()})
            actions = logits.argmax(-1).tolist()
            following = []
            for index, (slot, choice) in enumerate(zip(live, actions)):
                game, row = games[slot], rows[slot]
                legal = game.legal_actions()
                assert len(legal) == int(inputs["candidate_mask"][index].sum())
                action = legal[choice]
                before = game.search_state()
                row["action_counts"][action["type"]] += 1
                row["ended_with_play_available"] += int(action["type"] == "end_turn" and any(a["type"] == "play" for a in legal))
                row["actions"].append(dict(step=row["steps"], action=action, hp=before["hp"],
                                           energy=before["energy"], turn=before["turn"]))
                game.step(action)
                row["steps"] += 1
                after = game.search_state()
                assert game.fault == 0
                if after["dead"] or not after["combat"]:
                    row["outcome"] = "death" if after["dead"] else "combat_end"
                elif row["steps"] >= cap:
                    row["outcome"] = "capped"
                else:
                    following.append(slot)
                if "outcome" in row:
                    row.update(hp=after["hp"], turns=after["turn"], unused_potions=remaining_potions(game))
            live = following
        results.extend(rows)
        print(json.dumps(dict(completed=len(results), total=len(cases))), flush=True)
    return results, observations


def summarize(rows):
    counts = Counter(r["outcome"] for r in rows)
    return dict(cases=len(rows), seed_families=len({r["seed"] for r in rows}), outcomes=dict(counts),
                mean_hp=sum(r["hp"] for r in rows) / len(rows),
                potion_uses=sum(r["action_counts"].get("potion_use", 0) for r in rows),
                potion_discards=sum(r["action_counts"].get("potion_discard", 0) for r in rows),
                unused_potions_on_death=sum(r["unused_potions"] for r in rows if r["outcome"] == "death"),
                decisions=sum(r["steps"] for r in rows))


def compare(reference, current):
    for key in ("cases_sha256", "decision_cap", "selection", "engine_sha256"):
        assert reference[key] == current[key], key
    old = {r["case"]: r for r in reference["episodes"]}
    new = {r["case"]: r for r in current["episodes"]}
    assert old.keys() == new.keys()
    counts = Counter()
    deltas = []
    for key, a in old.items():
        b = new[key]
        if "capped" in (a["outcome"], b["outcome"]):
            counts["capped_pairs"] += 1
        else:
            aw, bw = a["outcome"] == "combat_end", b["outcome"] == "combat_end"
            counts["candidate_only_win" if bw and not aw else "reference_only_win" if aw and not bw else "both_win" if aw else "both_death"] += 1
        deltas.append(b["hp"] - a["hp"])
    return dict(counts=counts, mean_hp_delta=sum(deltas) / len(deltas))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--cases", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--trace-inputs", type=Path)
    parser.add_argument("--decision-cap", type=int, default=300)
    args = parser.parse_args()
    import spire

    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    checkpoint_hash = digest(args.checkpoint)
    cases = json.loads(args.cases.read_text())["cases"]
    assert cases and len({c["id"] for c in cases}) == len(cases) and args.decision_cap > 0
    model = load_policy(args.checkpoint)
    start = time.perf_counter()
    rows, observations = run(model, cases, args.decision_cap, 32 if args.trace_inputs else 0)
    result = dict(checkpoint=str(args.checkpoint), checkpoint_sha256=checkpoint_hash,
                  cases_sha256=digest(args.cases), engine_path=spire.__file__, engine_sha256=digest(spire.__file__),
                  code_commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                  selection="argmax", decision_cap=args.decision_cap, summary=summarize(rows),
                  elapsed_seconds=time.perf_counter() - start, episodes=rows)
    if args.reference:
        result["paired"] = compare(json.loads(args.reference.read_text()), result)
    assert digest(args.checkpoint) == checkpoint_hash
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
    if args.trace_inputs:
        with args.trace_inputs.open("xb") as stream:
            torch.save(observations, stream)
    print(json.dumps({k: v for k, v in result.items() if k != "episodes"}), flush=True)


if __name__ == "__main__":
    main()
