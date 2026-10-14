"""Generate host snapshot metadata from the fixed Java field inventory."""
import argparse
import json
import re
from pathlib import Path



def reward_templates(units, schema):
    potions = {}
    for unit in units:
        if unit["kind"] != "potion":
            continue
        fields = {"ID": unit["game_id"], "canUse": False, "potency": unit["facts"]["potency"] if isinstance(unit["facts"]["potency"], int) else None,
                  "rarity": unit["facts"]["rarity"], "slot": -1,
                  "targetRequired": unit["facts"]["target_required"]}
        assert set(fields) == set(schema["potions.AbstractPotion"]["fields"])
        potions[unit["game_id"]] = {"class": unit["java_class"].removeprefix("com.megacrit.cardcrawl."),
                                    "fields": {"AbstractPotion." + key: value for key, value in fields.items()}}
    return {"potions": potions}

def unit_snapshot_dispatch(root, units):
    lines = []
    for kind, enum, entity, variable, selector, context in (
        ("cards", "CardId", "Card", "card", "card.id", "bool master, int index"),
        ("powers", "PowerId", "Power", "power", "power.id", "int owner"),
        ("monsters", "MonsterId", "Monster", "monster", "monster.id", "int owner"),
        ("actions", "EffectKind", "Effect", "effect", "effect.kind", "int queue, int index"),
        ("events", "EventId", "Event", "event", "event.id", "int index"),
        ("relics", "RelicId", "Relic", "relic", "relic.id", "int index"),
    ):
        lines.append(f'template<class Graph> auto {kind}_snapshot(Graph& graph, const {entity}& {variable}, {context}, const std::string& key) -> decltype(graph.scalar(0)) {{')
        lines.append('const Game& game = graph.game; (void)game;')
        lines.append(f'switch ({selector}) {{')
        rows = re.findall(r'SPIRE_UNIT\((\w+), "([^\"]+)", "([^\"]+)", (\w+)\)', (root / "core" / kind / "catalog.def").read_text())
        for unit, _, header, typename in rows:
            source = (root / header).read_text()
            body = re.search(r'\bstruct\s+' + typename + r'\s*:[^{]+\{(.*?)^\};', source, re.M | re.S)[1]
            fields = re.findall(r'^\s*SPIRE_SNAPSHOT\("([^\"]+)",\s*(\w+)(?:, (.*))?\)\s*$', body, re.M)
            if not fields:
                continue
            lines.append(f'case {enum}::{unit}:')
            for field, method, args in fields:
                lines.append(f'if (key == "{field}") return graph.{method}({args});')
            lines.append('break;')
        lines.append('default: break; } return {}; }')
    lines.append('template<class Graph> void snapshot_statics(Graph& graph, pybind11::dict& result) { const Game& game = graph.game;')
    for kind in ('actions', 'powers', 'cards', 'monsters', 'relics', 'potions', 'events'):
        for _, game_id, header, _ in re.findall(r'SPIRE_UNIT\((\w+), "([^"]+)", "([^"]+)", (\w+)\)', (root / 'core' / kind / 'catalog.def').read_text()):
            for name, expression in re.findall(r'^\s*SPIRE_STATIC\("([^"]+)", (.*)\)\s*$', (root / header).read_text(), re.M):
                cls = game_id if kind == 'actions' else next(u['java_class'].removeprefix('com.megacrit.cardcrawl.') for u in units if u['kind'] == kind[:-1] and u['game_id'] == game_id)
                lines.append(f'result["{cls}"].cast<pybind11::dict>()["{name}"] = graph.scalar({expression});')
    lines.append('}')
    return '\n'.join(lines)

def generate(root, output):
    schema = json.loads((root / "inventory/snapshot-fields.json").read_text())
    unit_inventory = json.loads((root / "inventory/units.json").read_text())
    units = unit_inventory["units"]
    metadata = {}
    kinds = {"cards": "card", "powers": "power", "monsters": "monster", "relics": "relic", "potions": "potion"}
    for directory, kind in kinds.items():
        entries = []
        for game_id in re.findall(r'SPIRE_UNIT\([^,]+, "([^"]+)"', (root / "core" / directory / "catalog.def").read_text()):
            if kind == "potion" and game_id == "Potion Slot":
                entries.append({"class": "potions.PotionSlot"})
                continue
            unit = next(unit for unit in units if unit["kind"] == kind and unit["game_id"] == game_id)
            entry = {"class": unit["java_class"].removeprefix("com.megacrit.cardcrawl."), "id": game_id}
            if kind == "potion":
                entry["rarity"] = unit["facts"]["rarity"]
            if kind == "card":
                entry.update({key: unit["facts"][key] for key in ("color", "rarity", "target", "type", "tags")})
            if kind == "relic":
                entry["tier"] = unit["facts"]["tier"]
            entries.append(entry)
        metadata[directory] = entries
    output.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps({"schema": schema, "units": metadata, "rewards": reward_templates(units, schema), "globals": unit_inventory["snapshot_statics"]}, separators=(",", ":"))
    output.write_text('#pragma once\nnamespace spire::host {\nconstexpr char snapshot_metadata[] = R"schema(' + payload + ')schema";\n' + unit_snapshot_dispatch(root, units) + '\n}\n')


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.root, args.output)
