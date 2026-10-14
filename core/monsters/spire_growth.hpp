#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct SpireGrowth : MonsterHooks {
    SPIRE_SNAPSHOT("SpireGrowth.tackleDmg", scalar, 16)
    SPIRE_SNAPSHOT("SpireGrowth.smashDmg", scalar, 22)
    SPIRE_SNAPSHOT("SpireGrowth.constrictDmg", scalar, 10)
    SPIRE_SNAPSHOT("SpireGrowth.A_2_tackleDmg", scalar, 18)
    SPIRE_SNAPSHOT("SpireGrowth.A_2_smashDmg", scalar, 25)
    SPIRE_SNAPSHOT("SpireGrowth.tackleDmgActual", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("SpireGrowth.smashDmgActual", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_FN static constexpr MonsterData data() { return {190, 190, {18, 25}, 0, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) stats.hp_min = stats.hp_max = 170;
        if (g.run.ascension < 2) { stats.damage[0] = 16; stats.damage[1] = 22; }
        Monster m = make_monster(g, MonsterId::SpireGrowth, stats, hp_override);

        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        const Monster& m = g.monsters[owner];
        const bool constrict = !has_power(g, g.player.creature, PowerId::Constricted) && !last_move(g, m, 2);
        if (g.run.ascension >= 17 && constrict) set_monster_move(g, owner, 2, Intent::StrongDebuff);
        else if (num < 50 && !last_two_moves(g, m, 1)) set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0));
        else if (constrict) set_monster_move(g, owner, 2, Intent::StrongDebuff);
        else if (!last_two_moves(g, m, 3)) set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, m, 1));
        else set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 3) add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
        else if (m.move == 2) add_bottom(g, actions::AnimateSlowAttack::make(owner));
        else add_bottom(g, actions::AnimateFastAttack::make(owner));
        if (m.move == 2) queue_power(g, owner, player_target, PowerId::Constricted, (10) + (g.run.ascension >= 17 ? 2 : 0));
        else queue_monster_damage(g, owner, player_target, m.move == 1 ? 0 : 1);
        queue_monster_roll(g, owner);
    }
};
}
