#pragma once
#include "base.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Sentry : MonsterHooks {
    SPIRE_STATE(Sentry, bool firstMove;)
    SPIRE_SNAPSHOT("Sentry.beamDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("Sentry.dazedAmt", scalar, game.run.ascension >= 18 ? 3 : 2)
    SPIRE_FN static constexpr MonsterData data() { return {39, 45, {10, 0}, 0, 0, 1, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 8) { stats.hp_min = 38; stats.hp_max = 42; }
        if (g.run.ascension < 3) stats.damage[0] = 9;
        Monster m = make_monster(g, MonsterId::Sentry, stats, hp_override);
        m.state.Sentry.firstMove = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) { queue_power(g, owner, owner, PowerId::Artifact, 1); }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        Monster& m = g.monsters[owner];
        bool bolt = last_move(g, m, 4);
        if (m.state.Sentry.firstMove) {
            int position = -1;
            for (int i = 0; i < g.monster_order.size; ++i) if (g.monster_order[i] == owner) position = i;
            bolt = position % 2 == 0;
            m.state.Sentry.firstMove = false;
        }
        if (bolt) set_monster_move(g, owner, 3, Intent::Debuff);
        else set_monster_move(g, owner, 4, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        if (g.monsters[owner].move == 3) {
            add_bottom(g, actions::VFX::make(owner));
            queue_monster_card(g, CardId::Dazed, EffectKind::AddCardDiscard, g.run.ascension >= 18 ? 3 : 2);
        } else {
            add_bottom(g, actions::VFX::make());
            add_bottom(g, actions::VFX::make());
            queue_monster_damage(g, owner, player_target, 0);
        }
        queue_monster_roll(g, owner);
    }
};
}
