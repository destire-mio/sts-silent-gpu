#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct SpireGrowth : MonsterHooks {
    SPIRE_STATE(SpireGrowth, int tackleDmg; int smashDmg; int constrictDmg; int A_2_tackleDmg; int A_2_smashDmg; int tackleDmgActual; int smashDmgActual;)
    SPIRE_FN static constexpr MonsterData data() { return {190, 190, {18, 25}, 0, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) stats.hp_min = stats.hp_max = 170;
        if (g.run.ascension < 2) { stats.damage[0] = 16; stats.damage[1] = 22; }
        Monster m = make_monster(g, MonsterId::SpireGrowth, stats, hp_override);
        auto& s = m.state.SpireGrowth;
        s.tackleDmg = 16; s.smashDmg = 22; s.constrictDmg = 10;
        s.A_2_tackleDmg = 18; s.A_2_smashDmg = 25;
        s.tackleDmgActual = m.damage[0]; s.smashDmgActual = m.damage[1];
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        const Monster& m = g.monsters[owner];
        const bool constrict = !has_power(g, g.player.creature, PowerId::Constricted) && !last_move(m, 2);
        if (g.run.ascension >= 17 && constrict) set_monster_move(g, owner, 2, Intent::StrongDebuff);
        else if (num < 50 && !last_two_moves(m, 1)) set_monster_move(g, owner, 1, Intent::Attack, m.damage[0]);
        else if (constrict) set_monster_move(g, owner, 2, Intent::StrongDebuff);
        else if (!last_two_moves(m, 3)) set_monster_move(g, owner, 3, Intent::Attack, m.damage[1]);
        else set_monster_move(g, owner, 1, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 3) add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
        else if (m.move == 2) add_bottom(g, actions::AnimateSlowAttack::make(owner));
        else add_bottom(g, actions::AnimateFastAttack::make(owner));
        if (m.move == 2) queue_power(g, owner, player_target, PowerId::Constricted, m.state.SpireGrowth.constrictDmg + (g.run.ascension >= 17 ? 2 : 0));
        else queue_damage(g, owner, player_target, m.damage[m.move == 1 ? 0 : 1]);
        queue_monster_roll(g, owner);
    }
};
}
