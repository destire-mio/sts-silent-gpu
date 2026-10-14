#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinThief : MonsterHooks {
    SPIRE_NOT_APPLICABLE("escapeNext: Java defines the hook but contains no caller; the included escapeNext field remains false")
    SPIRE_STATE(GremlinThief, int16_t thiefDamage;)
    SPIRE_FN static constexpr MonsterData data() { return {11, 15, {10, 0}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 10; d.hp_max = 14; }
        if (g.run.ascension < 2) d.damage[0] = 9;
        Monster m = make_monster(g, MonsterId::GremlinThief, d, hp_override);
        m.state.GremlinThief.thiefDamage = monster_base_damage(g, m, 0);
        return m;
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
    SPIRE_FN static void deathReact(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.intent != Intent::Escape && !m.creature.dying) {
            set_monster_move(g, owner, 99, Intent::Escape);
            monster_createIntent(g, owner);
        }
    }
};
}
