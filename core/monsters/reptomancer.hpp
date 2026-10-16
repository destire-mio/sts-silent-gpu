#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/apply_power.hpp"

namespace spire::units::monsters {
struct Reptomancer : MonsterHooks {
    SPIRE_STATE(Reptomancer, int8_t daggers[4]; bool firstMove;)
    SPIRE_SNAPSHOT("Reptomancer.daggers", monster_array, monster.state.Reptomancer.daggers)
    SPIRE_SNAPSHOT("Reptomancer.daggersPerSpawn", scalar, game.run.ascension >= 18 ? 2 : 1)
    SPIRE_FN static constexpr MonsterData data() { return {190, 200, {16, 34}, 0, 0, 2, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        // Reptomancer.<init> rolls the super-constructor HP before its setHp roll.
        g.rng.monster_hp.random(180, 190);
        MonsterData stats = data();
        if (g.run.ascension < 8) { stats.hp_min = 180; stats.hp_max = 190; }
        if (g.run.ascension < 3) { stats.damage[0] = 13; stats.damage[1] = 30; }
        Monster m = make_monster(g, MonsterId::Reptomancer, stats, hp_override);
        for (int i = 0; i < 4; ++i) m.state.Reptomancer.daggers[i] = no_target;
        m.state.Reptomancer.firstMove = true;
        position_monster(m, -20.0f);
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        int position = -1;
        for (int i = 0; i < g.monster_order.size; ++i) if (g.monster_order[i] == owner) position = i;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int id = g.monster_order[i];
            if (g.monsters[id].id != g.monsters[owner].id) {
                Power power{PowerId::Minion, -1};
                power_constructPower(g, power, owner, owner, 0, -1);
                add_bottom(g, actions::ApplyPower::make(id, id, power, -1));
            }
            if (g.monsters[id].id == MonsterId::SnakeDagger) g.monsters[owner].state.Reptomancer.daggers[i > position ? 0 : 1] = id;
        }
    }
    SPIRE_FN static bool canSpawn(const Game& g, int owner) {
        int count = 0;
        for (int i = 0; i < g.monster_order.size; ++i) if (g.monster_order[i] != owner && !g.monsters[g.monster_order[i]].creature.dying) ++count;
        return count <= 3;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        if (m.state.Reptomancer.firstMove) {
            m.state.Reptomancer.firstMove = false;
            set_monster_move(g, owner, 2, Intent::Unknown);
            return;
        }
        // Reptomancer.getMove rerolls only a forbidden attack branch.
        while ((num < 33 && last_move(g, m, 1)) || (num >= 66 && last_move(g, m, 3)))
            num = num < 33 ? g.rng.ai.random(33, 99) : g.rng.ai.random(65);
        if (num < 33 || (num < 66 && (last_two_moves(g, m, 2) || !canSpawn(g, owner))))
            set_monster_move(g, owner, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0), 2);
        else if (num < 66) set_monster_move(g, owner, 2, Intent::Unknown);
        else set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, m, 1));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 1) {
            add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
            add_bottom(g, actions::VFX::make());
            queue_monster_damage(g, owner, player_target, 0);
            add_bottom(g, actions::VFX::make());
            queue_monster_damage(g, owner, player_target, 0);
            queue_power(g, owner, player_target, PowerId::Weak, 1, true);
        } else if (m.move == 2) {
            add_bottom(g, actions::ChangeState::make(g, owner, "SUMMON"));
            int spawned = 0;
            constexpr float positions[]{210.0f, -220.0f, 180.0f, -250.0f};
            for (int i = 0; spawned < (g.run.ascension >= 18 ? 2 : 1) && i < 4; ++i) {
                const int dagger = m.state.Reptomancer.daggers[i];
                if (dagger != no_target && alive(g.monsters[dagger].creature)) continue;
                m.state.Reptomancer.daggers[i] = queue_spawn_monster(g, MonsterId::SnakeDagger, positions[i], true);
                ++spawned;
            }
        } else if (m.move == 3) {
            add_bottom(g, actions::AnimateFastAttack::make(owner));
            add_bottom(g, actions::VFX::make());
            queue_monster_damage(g, owner, player_target, 1);
        }
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        monster_die_base(g, owner, trigger_relics);
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int id = g.monster_order[i];
            if (!g.monsters[id].creature.dead && !g.monsters[id].creature.dying) {
                Effect hide{}; hide.kind = EffectKind::HideHealthBar; hide.source = id; hide.target = no_target; add_top(g, hide);
                Effect e{}; e.kind = EffectKind::Suicide; e.target = id; add_top(g, e);
            }
        }
    }
};
}
