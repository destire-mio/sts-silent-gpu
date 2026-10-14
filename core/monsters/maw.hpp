#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Maw : MonsterHooks {
    SPIRE_STATE(Maw, int turnCount; bool roared;)
    SPIRE_SNAPSHOT("Maw.nomDmg", scalar, 5)
    SPIRE_SNAPSHOT("Maw.slamDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("Maw.strUp", scalar, game.run.ascension >= 17 ? 5 : 3)
    SPIRE_SNAPSHOT("Maw.terrifyDur", scalar, game.run.ascension >= 17 ? 5 : 3)
    SPIRE_FN static constexpr MonsterData data() { return {300, 300, {30, 5}, 0, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 2) stats.damage[0] = 25;
        Monster m = make_monster(g, MonsterId::Maw, stats, hp_override == 0 ? 300 : hp_override);
        auto& s = m.state.Maw;

        s.turnCount = 1;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        auto& s = m.state.Maw;
        ++s.turnCount;
        if (!s.roared) set_monster_move(g, owner, 2, Intent::StrongDebuff);
        else if (num < 50 && !last_move(g, m, 5)) set_monster_move(g, owner, 5, Intent::Attack, monster_base_damage(g, m, 1), s.turnCount / 2);
        else if (last_move(g, m, 3) || last_move(g, m, 5)) set_monster_move(g, owner, 4, Intent::Buff);
        else set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        auto& s = m.state.Maw;
        if (m.move == 2) {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e); add_bottom(g, actions::Shout::make(owner));
            queue_power(g, owner, player_target, PowerId::Weak, (g.run.ascension >= 17 ? 5 : 3), true);
            queue_power(g, owner, player_target, PowerId::Frail, (g.run.ascension >= 17 ? 5 : 3), true);
            s.roared = true;
        } else if (m.move == 3) { add_bottom(g, actions::AnimateSlowAttack::make(owner)); queue_monster_damage(g, owner, player_target, 0); }
        else if (m.move == 4) queue_power(g, owner, owner, PowerId::Strength, (g.run.ascension >= 17 ? 5 : 3));
        else {
            add_bottom(g, actions::AnimateSlowAttack::make(owner));
            for (int i = 0; i < s.turnCount / 2; ++i) {
                add_bottom(g, actions::VFX::make());
                queue_monster_damage(g, owner, player_target, 1);
            }
        }
        queue_monster_roll(g, owner);
    }
};
}
