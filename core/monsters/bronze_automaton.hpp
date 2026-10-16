#pragma once
#include "base.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct BronzeAutomaton : MonsterHooks {
    SPIRE_STATE(BronzeAutomaton, int numTurns; bool firstTurn;)
    SPIRE_SNAPSHOT("BronzeAutomaton.beamDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_SNAPSHOT("BronzeAutomaton.flailDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("BronzeAutomaton.blockAmt", scalar, game.run.ascension >= 9 ? 12 : 9)
    SPIRE_SNAPSHOT("BronzeAutomaton.strAmt", scalar, game.run.ascension >= 4 ? 4 : 3)
    SPIRE_FN static constexpr MonsterData data() { return {320, 320, {8, 50}, 0, 0, 2, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 4) { stats.damage[0] = 7; stats.damage[1] = 45; }
        stats.hp_min = stats.hp_max = g.run.ascension >= 9 ? 320 : 300;
        Monster m = make_monster(g, MonsterId::BronzeAutomaton, stats, hp_override);
        m.state.BronzeAutomaton.firstTurn = true;
        position_monster(m, -50.0f);
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) { queue_power(g, owner, owner, PowerId::Artifact, 3); }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        Monster& m = g.monsters[owner];
        auto& state = m.state.BronzeAutomaton;
        if (state.firstTurn) { state.firstTurn = false; set_monster_move(g, owner, 4, Intent::Unknown); }
        else if (state.numTurns == 4) { state.numTurns = 0; set_monster_move(g, owner, 2, Intent::Attack, monster_base_damage(g, m, 1)); }
        else if (last_move(g, m, 2)) set_monster_move(g, owner, g.run.ascension >= 19 ? 5 : 3, g.run.ascension >= 19 ? Intent::DefendBuff : Intent::Stun);
        else {
            if (last_move(g, m, 3) || last_move(g, m, 5) || last_move(g, m, 4)) set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0), 2);
            else set_monster_move(g, owner, 5, Intent::DefendBuff);
            ++state.numTurns;
        }
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 4) {
            queue_spawn_monster(g, MonsterId::BronzeOrb, -300.0f, true);
            const int right = queue_spawn_monster(g, MonsterId::BronzeOrb, 200.0f, true);
            g.monsters[right].state.BronzeOrb.count = 1;
        } else if (m.move == 1) {
            queue_monster_damage(g, owner, player_target, 0);
            queue_monster_damage(g, owner, player_target, 0);
        } else if (m.move == 5) {
            queue_block(g, owner, g.run.ascension >= 9 ? 12 : 9);
            queue_power(g, owner, owner, PowerId::Strength, g.run.ascension >= 4 ? 4 : 3);
        } else if (m.move == 2) {
            add_bottom(g, actions::VFX::make());
            queue_monster_damage(g, owner, player_target, 1);
        }
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        monster_die_base(g, owner, trigger_relics);
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
