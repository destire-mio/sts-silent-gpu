import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UNITS = {
    "cards": ("CardHooks", """
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::SelfAndEnemy, 0, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) { g.run.gold += 1; }
    static SPIRE_FN void triggerOnGainEnergy(Game& g, int, int amount, bool card_gain) {
        g.run.gold = g.run.gold * 10 + (card_gain ? amount : 1);
    }
"""),
    "powers": ("PowerHooks", """
    SPIRE_STATE(CatalogProbe, int16_t counter; bool active;)
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int) {
        ++p.state.CatalogProbe.counter;
        p.state.CatalogProbe.active = true;
        g.run.gold += 2 * p.state.CatalogProbe.counter;
    }
    static SPIRE_FN void onEnergyRecharge(Game& g, Power&, int) { g.run.gold = g.run.gold * 10 + 3; }
    static SPIRE_FN void onDamageAllEnemies(Game&, Power&, int, int* amounts) { amounts[0] += 2; }
"""),
    "relics": ("RelicHooks", """
    static SPIRE_FN constexpr RelicData data() { return {}; }
    static SPIRE_FN void atBattleStart(Game& g, int) { g.run.gold += 4; }
    static SPIRE_FN void onEnergyRecharge(Game& g, int) { g.run.gold = g.run.gold * 10 + 2; }
    static SPIRE_FN void onShuffle(Game& g, int) { g.run.gold += 100; }
"""),
    "potions": ("PotionHooks", """
    static SPIRE_FN constexpr PotionData data() { return {8}; }
    static SPIRE_FN void use(Game& g, int, int) { g.run.gold += 8; }
"""),
    "monsters": ("MonsterHooks", """
    static SPIRE_FN constexpr MonsterData data() { return {5, 5, {0, 0}, 0, 0}; }
    static SPIRE_FN Monster construct(Game& g, int hp) {
        return make_monster(g, MonsterId::CatalogProbe, data(), hp);
    }
    static SPIRE_FN void getMove(Game& g, int owner, int) {
        set_monster_move(g, owner, 1, Intent::Defend);
    }
    static SPIRE_FN void takeTurn(Game& g, int) { g.run.gold += 16; }
    static SPIRE_FN void changeState(Game& g, int owner, const char* state) {
        if (state[0] == 'T') g.monsters[owner].creature.block += 7;
    }
"""),
    "encounters": ("", """
    static SPIRE_FN void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::CatalogProbe));
        g.run.gold += 32;
    }
"""),
    "actions": ("ActionHooks", """
    static SPIRE_FN void execute(Game& g, const Effect&) { g.run.gold += 64; }
"""),
}

PROGRAM = r"""
#include "core/engine.hpp"
#include "host/names.hpp"
#include <cstdio>
using namespace spire;
int main() {
    Game g{};
    g.run.gold = 0;
    g.rng.generate_seeds(42);
    build_encounter(g, EncounterId::CatalogProbe);
    if (g.run.gold != 32 || g.monsters[0].id != MonsterId::CatalogProbe) return 1;
    monster_init(g, 0);
    if (g.monsters[0].pending_move != 1 || g.monsters[0].move != -1) return 15;
    g.cards.push_back(make_card(CardId::CatalogProbe));
    g.hand.push_back(0);
    const Screen previous = g.screen;
    g.screen = Screen::Combat;
    ActionSet legal{};
    legal_actions(g, legal);
    if (legal[0].kind != ActionKind::Play || legal[0].target != 0) return 23;
    g.screen = previous;
    use_card(g, 0, no_target);
    if (g.run.gold != 33) return 2;
    apply_power(g, player_target, 0, PowerId::CatalogProbe, 1);
    Effect start{};
    start.kind = EffectKind::MonsterStart;
    execute_effect(g, start);
    if (g.run.gold != 35) return 3;
    const auto& power_state = g.monsters[0].creature.powers.at(g.power_objects, 0).state.CatalogProbe;
    if (power_state.counter != 1 || !power_state.active) return 16;
    g.run.relics.push_back({RelicId::CatalogProbe, -1});
    relics_at_battle_start(g);
    if (g.run.gold != 39) return 4;
    g.run.potions[0] = PotionId::CatalogProbe;
    potion_use(g, 0, no_target);
    if (g.run.gold != 47) return 5;
    monster_take_turn(g, 0);
    if (g.run.gold != 63) return 6;
    Effect custom{};
    custom.kind = EffectKind::CatalogProbe;
    execute_effect(g, custom);
    if (g.run.gold != 127) return 7;
    if (host::parse_id<CardId>("CatalogProbe", host::card_names) != CardId::CatalogProbe) return 8;
    if (host::parse_id<PowerId>("CatalogProbe", host::power_names) != PowerId::CatalogProbe) return 9;
    if (host::parse_id<RelicId>("CatalogProbe", host::relic_names) != RelicId::CatalogProbe) return 10;
    if (host::parse_id<PotionId>("CatalogProbe", host::potion_names) != PotionId::CatalogProbe) return 11;
    if (host::parse_id<MonsterId>("CatalogProbe", host::monster_names) != MonsterId::CatalogProbe) return 12;
    if (host::parse_id<EncounterId>("CatalogProbe", host::encounter_names) != EncounterId::CatalogProbe) return 13;
    if (host::parse_id<EffectKind>("CatalogProbe", host::effect_names) != EffectKind::CatalogProbe) return 14;
    using namespace spire::units::actions;
    clear_actions(g, g.manager.actions);
    g.run.gold = 0;
    g.hand.push_back(0);
    Effect energy = GainEnergy::make(2);
    execute_effect(g, energy);
    if (g.run.gold != 2 || g.player.energy != 2 || g.manager.energy_gained_this_combat != 2) return 17;
    apply_power(g, player_target, player_target, PowerId::CatalogProbe, 1);
    g.run.gold = 0;
    g.manager.turn_has_ended = true;
    energy = GainEnergyAndEnableControls::make(2);
    execute_effect(g, energy);
    if (g.run.gold != 123 || g.player.energy != 4 || g.manager.turn_has_ended) return 18;
    Effect shuffle{};
    shuffle.kind = EffectKind::Shuffle;
    g.run.gold = 0;
    add_bottom(g, shuffle);
    if (g.run.gold != 100) return 19;
    shuffle = g.action_objects[g.manager.actions.pop_front()];
    execute_effect(g, shuffle);
    if (g.run.gold != 100) return 20;
    g.multi_damage.base[0] = 1;
    g.cards[0].multi_damage_count = 1;
    Effect all = DamageAllEnemies::from_card(g, 0);
    execute_effect(g, all);
    if (g.monsters[0].creature.hp != 2 || g.multi_damage.get(0).at(g.damage_exceptions, 0) != 3) return 21;
    Effect change = ChangeState::make(g, 0, "TEST");
    execute_effect(g, change);
    if (g.monsters[0].creature.block != 7 || !change.payload.state.ChangeState.called) return 22;
    std::puts("all seven catalog extensions dispatched");
}
"""


class CatalogExtension(unittest.TestCase):
    def run_command(self, command):
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        return result.stdout

    def test_header_and_catalog_are_sufficient(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            shutil.copytree(ROOT / "core", root / "core")
            (root / "inventory").mkdir()
            inventory = json.loads((ROOT / "inventory/units.json").read_text())
            inventory["units"].append({"kind": "card", "game_id": "CatalogProbe", "facts": {"rarity": "SPECIAL", "color": "GREEN", "name_order": [0, 1]}})
            (root / "inventory/units.json").write_text(json.dumps(inventory))
            originals = {p: p.read_bytes() for p in (root / "core").rglob("*") if p.is_file()}
            for kind, (base, methods) in UNITS.items():
                include = "base.hpp" if kind in ("monsters", "encounters") else "../hooks.hpp"
                inherit = " : " + base if base else ""
                (root / "core" / kind / "catalog_probe.hpp").write_text(
                    '#pragma once\n#include "' + include + '"\n'
                    + f"namespace spire::units::{kind} {{\nstruct CatalogProbe{inherit} {{\n"
                    + methods + "};\n}\n")
                catalog = root / "core" / kind / "catalog.def"
                with catalog.open("a") as file:
                    file.write(f'SPIRE_UNIT(CatalogProbe, "CatalogProbe", "core/{kind}/catalog_probe.hpp", CatalogProbe)\n')
            build = root / "build"
            self.run_command([sys.executable, str(ROOT / "host/generate_content.py"),
                              "--root", str(root), "--output", str(build / "spire_generated")])
            for path, original in originals.items():
                if path.name != "catalog.def":
                    self.assertEqual(original, path.read_bytes(), str(path.relative_to(root)))
            source = root / "probe.cpp"
            source.write_text(PROGRAM)
            executable = root / "probe"
            self.run_command([shutil.which("clang++"), "-std=c++17", "-DNDEBUG", "-Wall", "-Wextra",
                              "-Werror", "-ffp-contract=off", "-I" + str(root), "-I" + str(build),
                              "-I" + str(ROOT), str(source), "-o", str(executable)])
            self.assertEqual("all seven catalog extensions dispatched\n", self.run_command([str(executable)]))


if __name__ == "__main__":
    unittest.main()
