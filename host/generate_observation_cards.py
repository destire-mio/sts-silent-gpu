"""Extract numeric display-variable metadata, never localization text or assets."""
import argparse
import hashlib
import json
import re
import zipfile
from pathlib import Path


def generate(jar, root):
    assert hashlib.sha256(jar.read_bytes()).hexdigest() == "dd60a613a6178e08f1a57bcb8d5747e33c21fd49e743b88d76e9d74042803b2a"
    with zipfile.ZipFile(jar) as archive:
        cards = json.loads(archive.read("localization/eng/cards.json"))
    rows = re.findall(r'SPIRE_UNIT\((\w+), "([^"]+)"', (root / "core/cards/catalog.def").read_text())
    output = ['#pragma once', '#include "core/state.hpp"', '', 'namespace spire::obs {',
              '// AbstractCard.renderDescription/renderDynamicVariable uses only printed !D!, !B!, !M! variables.',
              'constexpr uint8_t card_variables[][2] = {']
    for _, java_id in rows:
        entry = cards[java_id]
        masks = []
        for text in [entry["DESCRIPTION"], entry.get("UPGRADE_DESCRIPTION", entry["DESCRIPTION"])]:
            text += " ".join(entry.get("EXTENDED_DESCRIPTION", []))
            masks.append(sum(1 << i for i, key in enumerate(["!D!", "!B!", "!M!"]) if key in text))
        output.append('    {' + ', '.join(map(str, masks)) + '},')
    output.extend(['};', 'static_assert(sizeof(card_variables) / sizeof(*card_variables) == static_cast<int>(CardId::Count));', '}'])
    (root / "obs/card_variables.hpp").write_text('\n'.join(output) + '\n')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jar", type=Path)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    generate(args.jar, args.root)
