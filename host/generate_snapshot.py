"""Generate host snapshot metadata from the fixed Java field inventory."""
import argparse
import json
import re
from pathlib import Path


def generate(root, output):
    schema = json.loads((root / "inventory/snapshot-fields.json").read_text())
    units = json.loads((root / "inventory/units.json").read_text())["units"]
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
            if kind == "card":
                entry.update({key: unit["facts"][key] for key in ("color", "rarity", "target", "type", "tags")})
                if game_id == "Strike_G":
                    entry["tags"] = ["STRIKE", "STARTER_STRIKE"]
            if kind == "relic":
                entry["tier"] = unit["facts"]["tier"]
            entries.append(entry)
        metadata[directory] = entries
    output.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps({"schema": schema, "units": metadata}, separators=(",", ":"))
    output.write_text('#pragma once\nnamespace spire::host {\nconstexpr char snapshot_metadata[] = R"schema(' + payload + ')schema";\n}\n')


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.root, args.output)
