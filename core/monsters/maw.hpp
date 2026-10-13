#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Maw : MonsterHooks {
    SPIRE_STATE(Maw, int nomDmg; int slamDmg; int strUp; int terrifyDur; int turnCount; bool roared;)
    SPIRE_FN static constexpr MonsterData data() { return {300, 300, {30, 5}, 0, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 2) stats.damage[0] = 25;
        Monster m = make_monster(g, MonsterId::Maw, stats, hp_override == 0 ? 300 : hp_override);
        auto& s = m.state.Maw;
        s.nomDmg = 5; s.slamDmg = m.damage[0];
        s.strUp = s.terrifyDur = g.run.ascension >= 17 ? 5 : 3;
        s.turnCount = 1;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        auto& s = m.state.Maw;
        ++s.turnCount;
        if (!s.roared) set_monster_move(g, owner, 2, Intent::StrongDebuff);
        else if (num < 50 && !last_move(m, 5)) set_monster_move(g, owner, 5, Intent::Attack, m.damage[1], s.turnCount / 2);
        else if (last_move(m, 3) || last_move(m, 5)) set_monster_move(g, owner, 4, Intent::Buff);
        else set_monster_move(g, owner, 3, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        auto& s = m.state.Maw;
        if (m.move == 2) {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e); add_bottom(g, actions::Shout::make(owner));
            queue_power(g, owner, player_target, PowerId::Weak, s.terrifyDur, true);
            queue_power(g, owner, player_target, PowerId::Frail, s.terrifyDur, true);
            s.roared = true;
        } else if (m.move == 3) { add_bottom(g, actions::AnimateSlowAttack::make(owner)); queue_damage(g, owner, player_target, m.damage[0]); }
        else if (m.move == 4) queue_power(g, owner, owner, PowerId::Strength, s.strUp);
        else {
            add_bottom(g, actions::AnimateSlowAttack::make(owner));
            for (int i = 0; i < s.turnCount / 2; ++i) {
                add_bottom(g, actions::VFX::make());
                queue_damage(g, owner, player_target, m.damage[1]);
            }
        }
        queue_monster_roll(g, owner);
    }
};
}
