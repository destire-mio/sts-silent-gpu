#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinWarrior : MonsterHooks {
    SPIRE_NOT_APPLICABLE("escapeNext: Java defines the hook but contains no caller; the included escapeNext field remains false")
    SPIRE_FN static constexpr MonsterData data() { return {21, 25, {5, 0}, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::GremlinWarrior, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 99) {
            Effect e{}; e.kind = EffectKind::Escape; e.target = index; add_bottom(g, e);
            queue_monster_move(g, index, 99, Intent::Escape); return;
        }
        queue_monster_damage(g, index, player_target, 0);
        if (m.escape_next) queue_monster_move(g, index, 99, Intent::Escape);
        else queue_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int index) {
        queue_power(g, index, index, PowerId::Angry, g.run.ascension >= 17 ? 2 : 1);
    }
    SPIRE_FN static void deathReact(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.intent != Intent::Escape && !m.creature.dying) {
            set_monster_move(g, owner, 99, Intent::Escape);
            monster_createIntent(g, owner);
        }
    }
};
}
