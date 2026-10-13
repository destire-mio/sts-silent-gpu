#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinFat : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {14, 18, {5, 0}, 0, 1}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::GremlinFat, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        set_monster_move(g, index, 2, Intent::AttackDebuff, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        queue_animation(g, index, EffectKind::AnimateSlowAttack);
        queue_damage(g, index, player_target, m.damage[0], DamageType::Normal, 0, AttackEffect::BluntLight);
        queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
        queue_power(g, index, player_target, PowerId::Frail, m.magic, true);
        queue_monster_roll(g, index);
    }
};
}
