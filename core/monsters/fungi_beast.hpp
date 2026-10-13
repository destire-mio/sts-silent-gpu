#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct FungiBeast : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {24, 28, {6, 0}, 0, 5}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::FungiBeast, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num < 60 && !last_two_moves(m, 1)) || (num >= 60 && last_move(m, 2)))
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 2, Intent::Buff);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 1) {
            queue_monster_state(g, index, MonsterStateChange::Attack);
            queue_wait(g, 0.5f);
            queue_damage(g, index, player_target, m.damage[0], DamageType::Normal, 0, AttackEffect::BluntLight);
        }
        else queue_power(g, index, index, PowerId::Strength, m.magic);
        queue_monster_roll(g, index);
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int index) {
        queue_power(g, index, index, PowerId::SporeCloud, 2);
    }
};
}
