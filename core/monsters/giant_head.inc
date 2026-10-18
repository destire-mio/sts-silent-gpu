#pragma once
#include "core/monsters/base.hpp"
#include "core/actions/change_state.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::monsters {
struct GiantHead : MonsterHooks {
    SPIRE_STATE(GiantHead, int count; int startingDeathDmg;)
    SPIRE_FN static constexpr MonsterData data() { return {520, 520, {13, 40, 45, 50, 55, 60, 65, 70}, 0, 0, 8, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 8) stats.hp_min = stats.hp_max = 500;
        if (g.run.ascension < 3) for (int i = 1; i < 8; ++i) stats.damage[i] -= 10;
        Monster m = make_monster(g, MonsterId::GiantHead, stats, hp_override);
        position_monster(m, -70.0f);
        m.state.GiantHead.count = 5;
        m.state.GiantHead.startingDeathDmg = monster_base_damage(g, m, 1);
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::Slow, 0);
        if (g.run.ascension >= 18) --g.monsters[owner].state.GiantHead.count;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        auto& s = m.state.GiantHead;
        if (s.count <= 1) {
            if (s.count > -6) --s.count;
            set_monster_move(g, owner, 2, Intent::Attack, s.startingDeathDmg - s.count * 5);
        } else {
            --s.count;
            if ((num < 50 && !last_two_moves(g, m, 1)) || (num >= 50 && last_two_moves(g, m, 3)))
                set_monster_move(g, owner, 1, Intent::Debuff);
            else set_monster_move(g, owner, 3, Intent::Attack, 13);
        }
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e); add_bottom(g, actions::Shout::make(owner));
        if (m.move == 1) queue_power(g, owner, player_target, PowerId::Weak, 1, true);
        else queue_monster_damage(g, owner, player_target, m.move == 3 ? 0 : min(7, 1 - m.state.GiantHead.count));
        queue_monster_roll(g, owner);
    }
};
}
