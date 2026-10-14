#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct AcidSlime_M : MonsterHooks {
    SPIRE_FN static constexpr MonsterData data() { return {29, 34, {8, 12}, 0, 1, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData d = data();
        if (g.run.ascension < 7) { d.hp_min = 28; d.hp_max = 32; }
        if (g.run.ascension < 2) { d.damage[0] = 7; d.damage[1] = 10; }
        return make_monster(g, MonsterId::AcidSlime_M, d, hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if (num < (g.run.ascension >= 17 ? 40 : 30)) {
            if (!last_two_moves(g, m, 1)) set_monster_move(g, index, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0));
            else {
                const bool attack = g.rng.ai.randomBoolean();
                if (attack) set_monster_move(g, index, 2, Intent::Attack, monster_base_damage(g, m, 1));
                else set_monster_move(g, index, 4, Intent::Debuff);
            }
        } else if (num < (g.run.ascension >= 17 ? 80 : 70)) {
            if (!(g.run.ascension >= 17 ? last_two_moves(g, m, 2) : last_move(g, m, 2))) set_monster_move(g, index, 2, Intent::Attack, monster_base_damage(g, m, 1));
            else if (g.rng.ai.randomBoolean(g.run.ascension >= 17 ? 0.5f : 0.4f))
                set_monster_move(g, index, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0));
            else set_monster_move(g, index, 4, Intent::Debuff);
        } else if (!(g.run.ascension >= 17 ? last_move(g, m, 4) : last_two_moves(g, m, 4))) set_monster_move(g, index, 4, Intent::Debuff);
        else if (g.rng.ai.randomBoolean(0.4f)) set_monster_move(g, index, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0));
        else set_monster_move(g, index, 2, Intent::Attack, monster_base_damage(g, m, 1));
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 4) queue_power(g, index, player_target, PowerId::Weak, data().magic, true);
        else {
            queue_damage(g, index, player_target, monster_base_damage(g, m, m.move == 2 ? 1 : 0));
            if (m.move == 1) queue_slimed(g, 1);
        }
        queue_monster_roll(g, index);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        monster_die_base(g, owner, trigger_relics);
        if (monsters_basically_dead(g) && (g.room_kind == RoomKind::Boss)) monster_onBossVictoryLogic(g, owner);
    }
};
}
