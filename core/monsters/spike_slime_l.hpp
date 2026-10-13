#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SpikeSlime_L : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {67, 73, {18, 0}, 0, 3}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        Monster m = make_monster(g, MonsterId::SpikeSlime_L, data(), hp_override);
        m.creature.powers.push_back(Power{PowerId::Split, -1});
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num < 30 && !last_two_moves(m, 1)) || (num >= 30 && last_move(m, 4)))
            set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
        else set_monster_move(g, index, 4, Intent::Debuff);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move != 3) queue_animation(g, index, EffectKind::AnimateSlowAttack);
        if (m.move == 3) {
            Effect e{};
            e.kind = EffectKind::Split;
            e.target = index;
            add_bottom(g, e);
            set_monster_move(g, index, 3, Intent::Unknown);
        } else if (m.move == 4) queue_power(g, index, player_target, PowerId::Frail, m.magic, true);
        else {
            queue_damage(g, index, player_target, m.damage[0], DamageType::Normal, 0, AttackEffect::BluntHeavy);
            queue_slimed(g, 2);
        }
        // SpikeSlime_L.takeTurn rolls even after splitting; AcidSlime_L does not.
        queue_monster_roll(g, index);
    }
    SPIRE_FN static void damage(Game& g, int index, const DamageInfo& info) {
        monster_damage_base(g, index, info);
        onDamage(g, index);
    }
    SPIRE_FN static void onDamage(Game& g, int index) {
        interrupt_for_split(g, index);
    }
    SPIRE_FN static void split(Game& g, int index) {
        split_into(g, index, MonsterId::SpikeSlime_M);
    }
};
}
