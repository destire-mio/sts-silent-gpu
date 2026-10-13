#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinWizard : MonsterHooks {
    SPIRE_STATE(GremlinWizard, int16_t currentCharge;)
    SPIRE_FN static constexpr MonsterData data() { return {22, 26, {30, 0}, 0, 0}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 21; stats.hp_max = 25; }
        if (g.run.ascension < 2) stats.damage[0] = 25;
        Monster m = make_monster(g, MonsterId::GremlinWizard, stats, hp_override);
        m.state.GremlinWizard.currentCharge = 1;
        position_monster(m, -35.0f);
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        (void)num;
        set_monster_move(g, index, 2, Intent::Unknown);
    }
    SPIRE_FN static void escapeNext(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (!m.cannot_escape && !m.escape_next) m.escape_next = true;
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 2) {
            ++m.state.GremlinWizard.currentCharge;
            if (m.escape_next) queue_monster_move(g, index, 99, Intent::Escape);
            else if (m.state.GremlinWizard.currentCharge == 3) queue_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
            else set_monster_move(g, index, 2, Intent::Unknown);
        } else if (m.move == 1) {
            m.state.GremlinWizard.currentCharge = 0;
            queue_damage(g, index, player_target, monster_base_damage(g, m, 0));
            if (m.escape_next) queue_monster_move(g, index, 99, Intent::Escape);
            else if (g.run.ascension >= 17) set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
            else set_monster_move(g, index, 2, Intent::Unknown);
        } else if (m.move == 99) {
            Effect e{};
            e.kind = EffectKind::Escape;
            e.target = index;
            add_bottom(g, e);
            queue_monster_move(g, index, 99, Intent::Escape);
        }
    }
    SPIRE_FN static void deathReact(Game& g, int index) {
        if (g.monsters[index].intent != Intent::Escape && !g.monsters[index].creature.dying) {
            set_monster_move(g, index, 99, Intent::Escape);
            monster_createIntent(g, index);
        }
    }
};
}
