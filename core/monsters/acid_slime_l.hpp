#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct AcidSlime_L : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {68, 72, {12, 18}, 0, 2, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        Monster m = make_monster(g, MonsterId::AcidSlime_L, data(), hp_override);
        m.creature.powers.push_back(Power{PowerId::Split, -1});
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if (num < 40) {
            if (!last_two_moves(m, 1)) set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
            else {
                const bool attack = g.rng.ai.randomBoolean(0.6f);
                if (attack) set_monster_move(g, index, 2, Intent::Attack, m.damage[1]);
                else set_monster_move(g, index, 4, Intent::Debuff);
            }
        } else if (num < 70) {
            if (!last_two_moves(m, 2)) set_monster_move(g, index, 2, Intent::Attack, m.damage[1]);
            else if (g.rng.ai.randomBoolean(0.6f))
                set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
            else set_monster_move(g, index, 4, Intent::Debuff);
        } else if (!last_move(m, 4)) set_monster_move(g, index, 4, Intent::Debuff);
        else if (g.rng.ai.randomBoolean(0.4f)) set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
        else set_monster_move(g, index, 2, Intent::Attack, m.damage[1]);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 3) {
            Effect e{};
            e.kind = EffectKind::Split;
            e.target = index;
            add_bottom(g, e);
            set_monster_move(g, index, 3, Intent::Unknown);
            return;
        }
        if (m.move == 4) queue_power(g, index, player_target, PowerId::Weak, m.magic, true);
        else {
            queue_damage(g, index, player_target, m.damage[m.move == 2 ? 1 : 0]);
            if (m.move == 1) queue_slimed(g, 2);
        }
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
        split_into(g, index, MonsterId::AcidSlime_M);
    }
};
}
