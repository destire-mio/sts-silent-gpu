import argparse
import json
from pathlib import Path
import sys

import replay

ROOT = Path(__file__).resolve().parents[1]
CASES = (
    ('Addict', (122, 145, 151)),
    ('BackToBasics', (108, 188, 209)),
    ('Beggar', (138, 227, 327)),
    ('Colosseum', (111, 135, 255)),
    ('CursedTome', (124, 185, 201)),
    ('DrugDealer', (114, 541, 568)),
    ('ForgottenAltar', (117, 123, 133)),
    ('Ghosts', (128, 207, 211)),
    ('Addict-poor', (122, 145, 151)),
    ('Addict-cost-boundary', (616, 903, 984)),
    ('BackToBasics-no-purge', (188, 276, 291)),
    ('BackToBasics-bottled-only', (188, 276, 291)),
    ('BackToBasics-all-upgraded', (108, 209, 394)),
    ('Beggar-cost-boundary', (227, 410, 457)),
    ('Colosseum-second-victory', (111, 135, 255)),
    ('CursedTome-ignore', (332, 344, 446)),
    ('CursedTome-book', (219, 483, 571)),
    ('CursedTome-only-Necronomicon', (219, 483, 571)),
    ('CursedTome-only-Enchiridion', (219, 483, 571)),
    ('CursedTome-only-NilrysCodex', (219, 483, 571)),
    ('CursedTome-all-books', (219, 483, 571)),
    ('CursedTome-all-books-circlet', (219, 483, 571)),
    ('CursedTome-hp-loss-hooks', (219, 483, 571)),
    ('CursedTome-lethal-page', (124, 185, 201)),
    ('DrugDealer-inject', (541, 768, 790)),
    ('DrugDealer-duplicate', (541, 768, 790)),
    ('DrugDealer-no-transform', (114, 541, 568)),
    ('DrugDealer-colorless', (568, 604, 726)),
    ('DrugDealer-curse', (568, 604, 726)),
    ('ForgottenAltar-idol', (117, 123, 133)),
    ('ForgottenAltar-duplicate-idol', (117, 123, 133)),
    ('ForgottenAltar-lethal-blood', (133, 139, 170)),
    ('ForgottenAltar-damage-hooks', (133, 139, 170)),
    ('Ghosts-odd-max', (211, 287, 343)),
    ('Ghosts-one-max', (211, 287, 343)),
    ('Ghosts-refuse', (128, 207, 415)),
    ('Colosseum-first-death', (111, 135, 255)),
    ('Colosseum-elite-hooks', (111, 135, 255)),
    ('CursedTome-lethal-book', (219, 483, 571)),
    ('BackToBasics-bottled-upgrade', (108, 209, 394)),
    ('CursedTome-one-book', (219, 483, 571)),
)


def jobs(output):
    return [dict(seed=seed, scenario=json.loads((ROOT / "tests/scenarios" / (name + ".json")).read_text()),
                 output=str(output.resolve() / f"{name}-{seed}.jsonl"))
            for name, seeds in CASES for seed in seeds]


def event(snapshot):
    room = snapshot["objects"][snapshot["room"]["ref"]]["fields"]
    reference = room["AbstractRoom.event"]
    return snapshot["objects"][reference["ref"]] if reference else None


def fields(snapshot, reference):
    return snapshot["objects"][reference["ref"]]["fields"]


def deck(snapshot):
    player = fields(snapshot, snapshot["player"])
    return [fields(snapshot, card) for card in fields(snapshot, player["AbstractPlayer.masterDeck"])["CardGroup.group"]]


def condition_branches(recordings):
    for name in ("Addict-poor", "DrugDealer-no-transform"):
        slot = 0 if name.startswith("Addict") else 1
        for rows in recordings[name]:
            snapshot = rows[1]["snapshot"]
            dialog = fields(snapshot, event(snapshot)["fields"]["AbstractEvent.imageEventText"])
            assert fields(snapshot, dialog["GenericEventDialog.optionList"][slot])["LargeDialogOptionButton.isDisabled"]
    for name in ("BackToBasics-no-purge", "BackToBasics-bottled-only"):
        for rows in recordings[name]:
            assert rows[1]["action"] == {"type": "choose", "option": 0}
            assert rows[2]["snapshot"]["screen"]["choice"] == "EVENT"
    for rows in recordings["BackToBasics-all-upgraded"]:
        snapshot = rows[2]["snapshot"]
        assert event(snapshot)["fields"]["BackToBasics.cardsUpgraded"] == []
    books = {"Necronomicon": "Necronomicon", "Enchiridion": "Enchiridion", "NilrysCodex": "NilrysCodex"}
    for suffix, cls in books.items():
        for rows in recordings["CursedTome-only-" + suffix]:
            snapshot = rows[6]["snapshot"]
            reward = fields(snapshot, fields(snapshot, snapshot["room"])["AbstractRoom.rewards"][0])
            assert snapshot["objects"][reward["RewardItem.relic"]["ref"]]["class"] == "relics." + cls
    for rows in recordings["CursedTome-all-books"]:
        snapshot = rows[6]["snapshot"]
        reward = fields(snapshot, fields(snapshot, snapshot["room"])["AbstractRoom.rewards"][0])
        assert snapshot["objects"][reward["RewardItem.relic"]["ref"]]["class"] == "relics.Circlet"
    for name, loss in (("Ghosts-odd-max", 34), ("Ghosts-one-max", 0)):
        for rows in recordings[name]:
            assert event(rows[1]["snapshot"])["fields"]["Ghosts.hpLoss"] == loss
            assert sum(card["AbstractCard.cardID"] == "Ghostly" for card in deck(rows[2]["snapshot"])) == 3
    assert any(row["snapshot"]["screen"]["type"] == "COMBAT_REWARD"
               and event(row["snapshot"])["fields"]["Colosseum.screen"] == "LEAVE"
               for rows in recordings["Colosseum-second-victory"] for row in rows[1:]
               if event(row["snapshot"]))
    for rows in recordings["Colosseum-first-death"]:
        assert event(rows[-1]["snapshot"])["fields"]["Colosseum.screen"] == "POST_COMBAT"
    for rows in recordings["CursedTome-lethal-book"]:
        assert rows[-1]["terminal"] == "death"
        assert event(rows[-1]["snapshot"])["fields"]["CursedTome.screen"] == "END"


def audit(output):
    choices = {}
    totals = {}
    recordings = {}
    for job, (name, _) in zip(jobs(output), [(name, seed) for name, seeds in CASES for seed in seeds]):
        path = Path(job["output"])
        with path.open() as stream:
            rows = [json.loads(line) for line in stream]
        assert rows[0]["seed"] == job["seed"] and rows[0]["scenario"] == job["scenario"], path
        assert rows[-1]["terminal"] in ("death", "victory"), path
        recordings.setdefault(name, []).append(rows)
        unit = name.split("-")[0]
        assert event(rows[1]["snapshot"])["class"] == "events.city." + unit, path
        count = totals.setdefault(unit, [0, 0, set()])
        count[0] += 1
        count[1] += len(rows) - 2
        count[2].add(job["seed"])
        for row in rows[1:-1]:
            snapshot = row["snapshot"]
            obj = event(snapshot)
            if not obj or obj["class"] != "events.city." + unit or snapshot["screen"]["choice"] != "EVENT":
                continue
            action = row["action"]
            if action["type"] != "choose":
                continue
            field = unit + (".screenNum" if unit in ("Addict", "DrugDealer", "Ghosts") else ".screen")
            screen = obj["fields"].get(field, obj["fields"]["AbstractEvent.screenNum"])
            choices.setdefault((unit, screen), set()).add(snapshot["screen"]["options"][action["option"]])
    required = {
        ("Addict", 0): {"option:0", "option:1", "option:2"},
        ("BackToBasics", "INTRO"): {"option:0", "option:1"},
        ("Beggar", "INTRO"): {"option:0", "option:1"},
        ("Beggar", "GAVE_MONEY"): {"option:0"},
        ("Colosseum", "POST_COMBAT"): {"option:0", "option:1"},
        ("CursedTome", "INTRO"): {"option:0", "option:1"},
        ("CursedTome", "LAST_PAGE"): {"option:0", "option:1"},
        ("DrugDealer", 0): {"option:0", "option:1", "option:2"},
        ("ForgottenAltar", 0): {"option:0", "option:1", "option:2"},
        ("Ghosts", 0): {"option:0", "option:1"},
    }
    for key, buttons in required.items():
        assert buttons <= choices.get(key, set()), (key, buttons, choices.get(key))
    condition_branches(recordings)
    for unit, (traces, decisions, seeds) in totals.items():
        assert len(seeds) >= 3, unit
        print(f"{unit}: {traces} traces, {decisions} decisions, {len(seeds)} seeds")
    return totals


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Record or replay the Act 2 event branch scenarios")
    parser.add_argument("command", choices=("record", "audit", "replay"))
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.command == "record":
        sys.path.insert(0, str(ROOT / "oracle"))
        from record import run_jobs
        run_jobs(jobs(args.output))
    if args.command == "replay":
        import spire
        for job in jobs(args.output):
            result = replay.replay(Path(job["output"]), spire)
            assert result.passed, (job["output"], result)
    audit(args.output)
