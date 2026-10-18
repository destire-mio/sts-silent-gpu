#pragma once
#include "core/monsters/base.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::monsters {
struct TheCollector : MonsterHooks {
    SPIRE_STATE(TheCollector, int turnsTaken; int8_t enemySlots[2]; bool initialSpawn; bool ultUsed;)
    SPIRE_SNAPSHOT("TheCollector.enemySlots", monster_map, monster.state.TheCollector.enemySlots, 1)
    SPIRE_SNAPSHOT("TheCollector.blockAmt", scalar, game.run.ascension >= 9 ? 18 : 15)
    SPIRE_SNAPSHOT("TheCollector.megaDebuffAmt", scalar, game.run.ascension >= 19 ? 5 : 3)
    SPIRE_SNAPSHOT("TheCollector.rakeDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("TheCollector.spawnX", scalar, -100.0f)
    SPIRE_SNAPSHOT("TheCollector.strAmt", scalar, game.run.ascension >= 19 ? 5 : game.run.ascension >= 4 ? 4 : 3)
    SPIRE_FN static constexpr MonsterData data() { return {300, 300, {21, 0}, 0, 0, 1, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 4) stats.damage[0] = 18;
        stats.hp_min = stats.hp_max = g.run.ascension >= 9 ? 300 : 282;
        Monster m = make_monster(g, MonsterId::TheCollector, stats, hp_override);
        m.state.TheCollector.initialSpawn = true;
        m.state.TheCollector.enemySlots[0] = m.state.TheCollector.enemySlots[1] = no_target;
        position_monster(m, 60.0f);
        return m;
    }
    SPIRE_FN static bool isMinionDead(const Game& g, int owner) {
        const auto& state = g.monsters[owner].state.TheCollector;
        for (int i = 0; i < 2; ++i) if (state.enemySlots[i] != no_target && g.monsters[state.enemySlots[i]].creature.dying) return true;
        return false;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        const auto& state = m.state.TheCollector;
        if (state.initialSpawn) set_monster_move(g, owner, 1, Intent::Unknown);
        else if (state.turnsTaken >= 3 && !state.ultUsed) set_monster_move(g, owner, 4, Intent::StrongDebuff);
        else if (num <= 25 && isMinionDead(g, owner) && !last_move(g, m, 5)) set_monster_move(g, owner, 5, Intent::Unknown);
        else if (num <= 70 && !last_two_moves(g, m, 2)) set_monster_move(g, owner, 2, Intent::Attack, monster_base_damage(g, m, 0));
        else if (!last_move(g, m, 3)) set_monster_move(g, owner, 3, Intent::DefendBuff);
        else set_monster_move(g, owner, 2, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        auto& state = m.state.TheCollector;
        if (m.move == 1) {
            for (int i = 0; i < 2; ++i) state.enemySlots[i] = queue_spawn_monster(g, MonsterId::TorchHead, -100.0f - 185.0f * static_cast<float>(i + 1), true);
            state.initialSpawn = false;
        } else if (m.move == 2) queue_monster_damage(g, owner, player_target, 0);
        else if (m.move == 3) {
            queue_block(g, owner, (g.run.ascension >= 9 ? 18 : 15) + (g.run.ascension >= 19 ? 5 : 0));
            for (int i = 0; i < g.monster_order.size; ++i) {
                const int id = g.monster_order[i];
                if (!g.monsters[id].creature.dead && !g.monsters[id].creature.dying && !g.monsters[id].creature.escaped)
                    queue_power(g, owner, id, PowerId::Strength, g.run.ascension >= 19 ? 5 : g.run.ascension >= 4 ? 4 : 3);
            }
        } else if (m.move == 4) {
            add_bottom(g, actions::Talk::make(owner));
            Effect sound{}; sound.kind = EffectKind::SFX; add_bottom(g, sound);
            add_bottom(g, actions::VFX::make());
            const int amount = g.run.ascension >= 19 ? 5 : 3;
            queue_power(g, owner, player_target, PowerId::Weak, amount, true);
            queue_power(g, owner, player_target, PowerId::Vulnerable, amount, true);
            queue_power(g, owner, player_target, PowerId::Frail, amount, true);
            state.ultUsed = true;
        } else if (m.move == 5) {
            for (int i = 0; i < 2; ++i) if (g.monsters[state.enemySlots[i]].creature.dying)
                state.enemySlots[i] = queue_spawn_monster(g, MonsterId::TorchHead, -100.0f - 185.0f * static_cast<float>(i + 1), true);
        }
        ++state.turnsTaken;
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        monster_die_base(g, owner, trigger_relics);
        // TheCollector.die adds 1.5s before AbstractMonster.die and onBossVictoryLogic.
        g.death_wait_bound[owner] += 90;
        monster_onBossVictoryLogic(g, owner);
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int id = g.monster_order[i];
            if (!g.monsters[id].creature.dead && !g.monsters[id].creature.dying) {
                Effect hide{}; hide.kind = EffectKind::HideHealthBar; hide.source = id; hide.target = no_target; add_top(g, hide);
                Effect e{}; e.kind = EffectKind::Suicide; e.target = id; add_top(g, e);
                add_top(g, actions::VFX::make(id));
            }
        }
    }
};
}
