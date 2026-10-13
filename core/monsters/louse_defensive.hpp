#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct LouseDefensive : MonsterHooks {
    SPIRE_STATE(LouseDefensive, bool isOpen;)
    SPIRE_FN static constexpr MonsterData data() { return {12, 18, {0, 0}, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        Monster m = make_monster(g, MonsterId::LouseDefensive, data(), hp_override);
        m.state.LouseDefensive.isOpen = true;
        m.damage[0] = g.rng.monster_hp.random(6, 8);
        m.damage_output[0] = m.damage[0];
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num < 25 && !last_move(m, 4)) || (num >= 25 && last_two_moves(m, 3)))
            set_monster_move(g, index, 4, Intent::Debuff);
        else set_monster_move(g, index, 3, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 3) {
            if (!m.state.LouseDefensive.isOpen) {
                queue_monster_state(g, index, MonsterStateChange::Open);
                queue_wait(g, 0.5f);
            }
            queue_animation(g, index, EffectKind::AnimateSlowAttack);
            queue_damage(g, index, player_target, m.damage[0], DamageType::Normal, 0, AttackEffect::BluntLight);
        } else {
            const bool open = m.state.LouseDefensive.isOpen;
            queue_monster_state(g, index, open ? MonsterStateChange::RearIdle : MonsterStateChange::Rear);
            queue_wait(g, open ? 0.9f : 1.2f);
            queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
        }
        queue_monster_roll(g, index);
    }
    SPIRE_FN static void changeState(Game& g, int index, int state) {
        g.monsters[index].state.LouseDefensive.isOpen = state != static_cast<int>(MonsterStateChange::Closed);
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int index) {
        queue_power(g, index, index, PowerId::CurlUp, g.rng.monster_hp.random(9, 12));
    }
};
}
