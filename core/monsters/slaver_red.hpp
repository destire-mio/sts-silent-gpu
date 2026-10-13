#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SlaverRed : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {48, 52, {14, 9}, 0, 2, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::SlaverRed, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if (m.first_move) {
            m.first_move = false;
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        } else if (num >= 75 && !m.used_special) set_monster_move(g, index, 2, Intent::StrongDebuff);
        else if ((num >= 55 && m.used_special && !last_two_moves(m, 1)) || last_move(m, 3))
            set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, index, 3, Intent::AttackDebuff, m.damage[1]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 2) {
            queue_monster_state(g, index, MonsterStateChange::UseNet);
            queue_power(g, index, player_target, PowerId::Entangled, 1, true);
            m.used_special = true;
        } else {
            queue_animation(g, index, EffectKind::AnimateSlowAttack);
            queue_damage(g, index, player_target, m.damage[m.move == 1 ? 0 : 1], DamageType::Normal, 0, m.move == 1 ? AttackEffect::SlashHorizontal : AttackEffect::SlashDiagonal);
            if (m.move == 3) queue_power(g, index, player_target, PowerId::Vulnerable, m.magic, true);
        }
        queue_monster_roll(g, index);
    }
};
}
