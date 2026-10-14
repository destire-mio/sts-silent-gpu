#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SlimeBoss : MonsterHooks {
    SPIRE_STATE(SlimeBoss, bool firstTurn;)
    SPIRE_SNAPSHOT("SlimeBoss.tackleDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("SlimeBoss.slamDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_FN static constexpr MonsterData data() { return {150, 150, {10, 38}, 0, 0, 2, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 4) { stats.damage[0] = 9; stats.damage[1] = 35; }
        stats.hp_min = stats.hp_max = g.run.ascension >= 9 ? 150 : 140;
        Monster m = make_monster(g, MonsterId::SlimeBoss, stats, hp_override);
        m.state.SlimeBoss.firstTurn = true;
        Power split{PowerId::Split, -1};
        split.instance = next_power_identity(g);
        power_initialize_state(split);
        m.creature.powers.push_back(g.power_objects, g.power_objects.add(split));
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        if (g.monsters[owner].state.SlimeBoss.firstTurn) {
            g.monsters[owner].state.SlimeBoss.firstTurn = false;
            set_monster_move(g, owner, 4, Intent::StrongDebuff);
        }
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 4) {
            queue_slimed(g, g.run.ascension >= 19 ? 5 : 3);
            set_monster_move(g, owner, 2, Intent::Unknown);
        } else if (m.move == 2) set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 1));
        else if (m.move == 1) {
            queue_monster_damage(g, owner, player_target, 1);
            set_monster_move(g, owner, 4, Intent::StrongDebuff);
        } else if (m.move == 3) {
            queue_split(g, owner, MonsterId::SpikeSlime_L, MonsterId::AcidSlime_L, -385.0f, 120.0f);
            set_monster_move(g, owner, 3, Intent::Unknown);
        }
    }
    SPIRE_FN static void damage(Game& g, int owner, const DamageInfo& info) {
        monster_damage_base(g, owner, info);
        Monster& m = g.monsters[owner];
        if (!m.creature.dying && static_cast<float>(m.creature.hp) <= static_cast<float>(m.creature.max_hp) / 2.0f && m.move != 3) {
            set_monster_move(g, owner, 3, Intent::Unknown);
            monster_createIntent(g, owner);
            queue_monster_move(g, owner, 3, Intent::Unknown);
        }
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        monster_die_base(g, owner, trigger_relics);
        for (int i = 0; i < g.manager.actions.size; ++i)
            if (g.action_objects[g.manager.actions[i]].kind == EffectKind::SpawnMonster) return;
        if (g.monsters[owner].creature.hp <= 0) monster_onBossVictoryLogic(g, owner);
    }
};
}
