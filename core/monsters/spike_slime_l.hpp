#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SpikeSlime_L : MonsterHooks {
    SPIRE_STATE(SpikeSlime_L, bool splitTriggered;)
    SPIRE_SNAPSHOT("SpikeSlime_L.saveX", scalar, monster.offset_x)
    SPIRE_FN static constexpr MonsterData data() { return {67, 73, {18, 0}, 0, 3}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 64; stats.hp_max = 70; }
        if (g.run.ascension < 2) { stats.damage[0] = 16; stats.damage[1] = 0; }
        Monster m = make_monster(g, MonsterId::SpikeSlime_L, stats, hp_override);
        Power split{PowerId::Split, -1};
        split.instance = g.next_power_instance++;
        power_initialize_state(split);
        m.creature.powers.push_back(g.power_objects, g.power_objects.add(split));
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        if ((num < 30 && !last_two_moves(g, m, 1)) || (num >= 30 && (g.run.ascension >= 17 ? last_move(g, m, 4) : last_two_moves(g, m, 4))))
            set_monster_move(g, index, 1, Intent::AttackDebuff, m.damage[0]);
        else set_monster_move(g, index, 4, Intent::Debuff);
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 3) {
            queue_split(g, index, MonsterId::SpikeSlime_M, MonsterId::SpikeSlime_M, m.offset_x - 134.0f, m.offset_x + 134.0f);
            set_monster_move(g, index, 3, Intent::Unknown);
        } else if (m.move == 4) queue_power(g, index, player_target, PowerId::Frail, g.run.ascension >= 17 ? 3 : 2, true);
        else {
            queue_damage(g, index, player_target, m.damage[0]);
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
        Monster& m = g.monsters[index];
        if (!m.creature.dying && static_cast<float>(m.creature.hp) <= static_cast<float>(m.creature.max_hp) / 2.0f &&
            m.move != 3 && !m.state.SpikeSlime_L.splitTriggered) {
            set_monster_move(g, index, 3, Intent::Unknown);
            monster_createIntent(g, index);
            queue_monster_move(g, index, 3, Intent::Unknown);
            m.state.SpikeSlime_L.splitTriggered = true;
        }
    }
};
}
