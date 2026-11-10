"""Replay a frozen E4 exam and record decisions, fights, and terminal inventory."""
import argparse
from collections import Counter
import gzip
import json
from pathlib import Path
import time

import torch

from tools.e4_value_truth import EXAM_SEEDS, collector, load_policy
from train.e4 import ACT, ACTION_KINDS, COMBAT_PHASE, DEATH, FLOOR, HP, PHASE, ROOM, SCREEN, SCREENS
from train.e4_evaluation_ledger import CARDS, POTIONS, RELICS, visible_state
from train.e4_rewards import REST_OPTIONS
from train.e4_stage12 import NoSLCollector


ROOMS = ("monster", "elite", "boss", "shop", "rest", "treasure",
         "event", "empty", "neow", "boss_treasure", "victory", "heart_event")
CHOOSE, POTION_USE, SKIP = 3, 4, 7
SHOP, REST = 3, 4
OFFER_NAMES = {3: CARDS, 6: RELICS, 7: POTIONS}


class DetailCollector(NoSLCollector):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.details = [None] * self.search.lanes
        self.open_fights = [None] * self.search.lanes
        self.turns = torch.empty(self.search.size, device=self.search.device, dtype=torch.int64)

    def reset(self, slots, seeds):
        super().reset(slots, seeds)
        for slot, seed in zip(slots, seeds):
            self.details[slot] = dict(seed=seed, fights=[], card_rewards=[], shop=[], campfire=[], map=[])
            self.open_fights[slot] = None

    def action_tokens(self, slot, actions):
        arrays = self.search.arrays
        refs = arrays["candidate_refs"][slot, :, actions].long()
        kinds = arrays["candidate_kinds"][slot, actions].tolist()
        indexes = (refs - 1).clamp_min(0)
        tags = arrays["tags"][slot, 0].long()[indexes].tolist()
        ids = arrays["ids"][slot, 0].long()[indexes].tolist()
        values = arrays["values"][slot, :2].long()[:, indexes].tolist()
        return [(ACTION_KINDS[kind], [
            (tags[j][i], ids[j][i], values[0][j][i], values[1][j][i]) if ref else None
            for j, ref in enumerate(row)])
                for i, (kind, row) in enumerate(zip(kinds, refs.T.tolist()))]

    def advance(self, slots, actions, progress):
        search = self.search
        good = [slot for slot in slots if slot not in search.failed]
        starts = [slot for slot in good if progress[slot][PHASE] == COMBAT_PHASE
                  and self.open_fights[slot] is None]
        for slot in starts:
            p = progress[slot]
            row = dict(act=p[ACT], floor=p[FLOOR], room=ROOMS[p[ROOM]],
                       encounter=visible_state(search, slot)["encounter"], entry_hp=p[HP], potions=[])
            self.details[slot]["fights"].append(row)
            self.open_fights[slot] = row

        chosen_kinds = search.arrays["candidate_kinds"][slots, actions].tolist()
        potions, shop = {}, {}
        for slot, action, kind in zip(slots, actions, chosen_kinds):
            if slot in search.failed:
                continue
            p, detail = progress[slot], self.details[slot]
            screen = SCREENS[p[SCREEN]]
            position = dict(act=p[ACT], floor=p[FLOOR], decision=len(self.paths[slot]))
            if self.open_fights[slot] is not None and kind == POTION_USE:
                _, (item, target, _) = self.action_tokens(slot, [action])[0]
                potions[slot] = dict(potion=POTIONS[item[1]], target=target[1] if target else None,
                                     decision=position["decision"])
            if screen == "card_reward":
                count = int(search.arrays["candidate_counts"][slot])
                options = self.action_tokens(slot, list(range(count)))
                offered = [dict(card=CARDS[t[1]], upgrades=t[2])
                           for _, (_, _, t) in options if t and t[0] == 3]
                _, (_, _, choice) = options[action]
                picked = dict(card=CARDS[choice[1]], upgrades=choice[2]) if choice and choice[0] == 3 else None
                detail["card_rewards"].append(dict(**position, offered=offered, picked=picked,
                    action="skip" if kind == SKIP else "take" if picked else "singing_bowl"))
            elif screen == "map" and kind in (CHOOSE, SKIP):
                _, (_, _, destination) = self.action_tokens(slot, [action])[0]
                node = [destination[2], destination[3]] if destination and destination[0] == 10 else None
                detail["map"].append(dict(**position, node=node, boss=bool(destination and destination[0] == 11),
                                          action=ACTION_KINDS[kind]))
            elif p[ROOM] == REST and screen == "room" and kind == CHOOSE:
                rest = search.frames([slot], progress)[slot]["rest_options"][action]
                if rest is not None:
                    detail["campfire"].append(dict(**position, choice=REST_OPTIONS[rest - 1]))
            elif p[ROOM] == SHOP and screen in ("shop", "grid") and kind == CHOOSE:
                _, (_, _, offer) = self.action_tokens(slot, [action])[0]
                names = OFFER_NAMES.get(offer[0], ()) if offer else ()
                selected = names[offer[1]] if names else "purge" if screen == "shop" and offer else None
                shop[slot] = position, screen, selected, visible_state(search, slot)
        if potions:
            search.batch.combat_turns_tensors(self.turns)
            for slot, row in potions.items():
                row["turn"] = int(self.turns[slot])
                self.open_fights[slot]["potions"].append(row)

        count = len(self.completed)
        rewards, terminal = super().advance(slots, actions, progress)
        ended = {slot for slot, done in zip(slots, terminal) if done}
        after = dict(progress)
        after.update(search.last_step_progress)
        for slot, (position, screen, selected, before) in shop.items():
            if slot in search.failed:
                continue
            current = visible_state(search, slot)
            old_deck = Counter((c["card_id"], c["upgraded"]) for c in before["deck"])
            new_deck = Counter((c["card_id"], c["upgraded"]) for c in current["deck"])
            removed = [dict(card=card, upgraded=upgraded) for card, upgraded in (old_deck-new_deck).elements()]
            paid = before["gold"] - current["gold"]
            if paid > 0 or removed:
                self.details[slot]["shop"].append(dict(**position, selected=selected, gold_spent=paid,
                                                       removed=removed, screen=screen))

        closed = [slot for slot in slots if self.open_fights[slot] is not None and
                  (slot in ended or after[slot][PHASE] != COMBAT_PHASE)]
        if closed:
            search.batch.combat_turns_tensors(self.turns)
        for slot in closed:
            row = self.open_fights[slot]
            outcome = "death" if after[slot][SCREEN] == DEATH else "stalled" if slot in search.failed or (
                slot in ended and after[slot][PHASE] == COMBAT_PHASE) else "left_combat"
            row.update(exit_hp=after[slot][HP], turns=int(self.turns[slot]), outcome=outcome)
            self.open_fights[slot] = None
        for slot, episode in zip((s for s in slots if s in ended), self.completed[count:]):
            detail = self.details[slot]
            detail.update(final_floor=episode["floor"], outcome=episode["outcome"], reach=episode["reach"],
                          bosses_seen=episode["bosses_seen"], candidate_indices=episode["candidates"])
            if episode["outcome"] == "death":
                final = visible_state(search, slot)
                detail["death"] = dict(act=after[slot][ACT], floor=after[slot][FLOOR],
                                       room=ROOMS[after[slot][ROOM]],
                                       deck=final["deck"], relics=final["relics"],
                                       potions=final["potions"])
            episode["detail"] = detail
        return rewards, terminal


def load_references(paths):
    result = {}
    for path in paths:
        opener = gzip.open if path.suffix == ".gz" else open
        with opener(path, "rt", encoding="utf-8") as file:
            rows = json.load(file)
        if isinstance(rows, dict):
            rows = rows["episodes"]
        for row in rows:
            seed = row["seed"]
            if seed in result:
                raise ValueError(f"Duplicate reference seed {seed}")
            result[seed] = row["floor"] if "floor" in row else row["final_floor"]
    return result


def verify_floors(rows, reference):
    for row in rows:
        seed = row["seed"]
        if row["floor"] != reference[seed]:
            raise ValueError(f"Seed {seed}: replay floor {row['floor']} differs from exam floor {reference[seed]}")


def report(rows, checked):
    causes, entry_hp, unused = Counter(), Counter(), Counter()
    for row in rows:
        if "death" not in row:
            continue
        last = row["fights"][-1] if row["fights"] and row["fights"][-1]["outcome"] == "death" else None
        death = row["death"]
        encounter = ", ".join(last["encounter"]) if last else "outside combat"
        causes[(death["act"], death["floor"], death["room"], encounter)] += 1
        if last is not None:
            lower = max(last["entry_hp"], 0) // 10 * 10
            entry_hp[lower] += 1
        unused[sum(p != "Potion Slot" for p in death["potions"])] += 1
    lines = ["# E4 exam replay", "", f"Replays {len(rows)}. Initial floor matches {checked}.",
             f"Deaths {sum(causes.values())}. Stalled {sum(r['outcome'] == 'stalled' for r in rows)}.", "",
             "| Act | Floor | Room | Encounter | Deaths |", "| ---: | ---: | --- | --- | ---: |"]
    for (act, floor, room, encounter), count in sorted(causes.items(), key=lambda item: (-item[1], item[0])):
        lines.append(f"| {act} | {floor} | {room} | {encounter} | {count} |")
    lines += ["", "| Fight entry HP | Deaths |", "| ---: | ---: |"]
    lines += [f"| {hp}–{hp+9} | {count} |" for hp, count in sorted(entry_hp.items())]
    lines += ["", "| Unused potions at death | Deaths |", "| ---: | ---: |"]
    lines += [f"| {count} | {deaths} |" for count, deaths in sorted(unused.items())]
    return "\n".join(lines) + "\n"


def replay(model, seeds, reference, device, lanes, deadline, verify_count=8):
    if not seeds or len(seeds) != len(set(seeds)):
        raise ValueError("Supply distinct exam seeds")
    if missing := set(seeds).difference(reference):
        raise ValueError(f"Missing reference for {len(missing)} selected seeds")
    checked = min(verify_count, len(seeds))
    first = collector(lanes, device, 101, deadline, greedy=True)
    verify_floors(first.evaluate(model, seeds[:checked]), reference)
    del first
    detail = collector(lanes, device, 101, deadline, greedy=True, cls=DetailCollector)
    episodes = detail.evaluate(model, seeds)
    verify_floors(episodes, reference)
    return sorted((e["detail"] for e in episodes), key=lambda row: row["seed"]), checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("--reference", nargs="+", required=True, type=Path, help="Original exam ledger or both original rank shards")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--seeds", nargs="+", type=int, default=list(EXAM_SEEDS))
    parser.add_argument("--device", default="cuda:0")
    parser.add_argument("--lanes", type=int, default=512)
    parser.add_argument("--fp32", action="store_true")
    args = parser.parse_args()
    if args.lanes < 1:
        parser.error("Lane count must be positive")
    torch.set_num_threads(1)
    device = torch.device(args.device)
    model = load_policy(args.checkpoint, device, args.fp32)
    rows, checked = replay(model, args.seeds, load_references(args.reference), device, args.lanes,
                           time.monotonic() + 3600)
    args.output.mkdir()
    with gzip.open(args.output / "exam-detail.jsonl.gz", "wt", encoding="utf-8") as stream:
        for row in rows:
            stream.write(json.dumps(row, separators=(",", ":"), allow_nan=False) + "\n")
    (args.output / "exam-detail.md").write_text(report(rows, checked), encoding="utf-8")


if __name__ == "__main__":
    main()
