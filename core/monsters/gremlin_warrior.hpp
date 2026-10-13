#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct GremlinWarrior : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {21, 25, {5, 0}, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        return make_monster(g, MonsterId::GremlinWarrior, data(), hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        queue_animation(g, index, EffectKind::AnimateSlowAttack);
        queue_damage(g, index, player_target, m.damage[0], DamageType::Normal, 0, AttackEffect::SlashDiagonal);
        queue_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int index) {
        queue_power(g, index, index, PowerId::Angry, g.monsters[index].magic);
    }
};
}
