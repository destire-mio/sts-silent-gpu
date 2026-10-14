#pragma once
#include "combat.hpp"
#include "spire_generated/content.hpp"

namespace spire {
SPIRE_FN inline MonsterData monster_stats(MonsterId id) { return monster_data(id); }
SPIRE_FN inline Monster construct_monster(Game& g, MonsterId id, int hp_override) {
    return monster_construct(g, id, hp_override);
}
SPIRE_FN inline void get_monster_move(Game& g, int index, int num) { monster_getMove(g, index, num); }
SPIRE_FN inline void monster_take_turn(Game& g, int index) { monster_takeTurn(g, index); }
SPIRE_FN inline void monster_pre_battle(Game& g, int index) { monster_usePreBattleAction(g, index); }
SPIRE_FN inline void monster_on_damage(Game& g, int index) { monster_onDamage(g, index); }
SPIRE_FN inline void build_encounter(Game& g, EncounterId encounter) {
    g.monsters.clear();
    g.monster_order.clear();
    encounter_build(g, encounter);
}
}
