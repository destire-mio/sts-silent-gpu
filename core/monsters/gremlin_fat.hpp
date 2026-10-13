#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinFat : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {14, 18, {5, 0}, 0, 1}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 13; d.hp_max = 17; }
        if (g.run.ascension < 2) d.damage[0] = 4;
        return make_monster(g, MonsterId::GremlinFat, d, hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        set_monster_move(g, index, 2, Intent::AttackDebuff, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 99) {
            Effect e{}; e.kind = EffectKind::Escape; e.target = index; add_bottom(g, e);
            queue_monster_move(g, index, 99, Intent::Escape); return;
        }
        queue_damage(g, index, player_target, monster_base_damage(g, m, 0));
        queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
        if (g.run.ascension >= 17) queue_power(g, index, player_target, PowerId::Frail, m.magic, true);
        if (m.escape_next) queue_monster_move(g, index, 99, Intent::Escape);
        else queue_monster_roll(g, index);
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
