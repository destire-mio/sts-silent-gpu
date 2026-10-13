#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::monsters {
struct Hexaghost : MonsterHooks {
    SPIRE_STATE(Hexaghost, int orbActiveCount; bool activated; bool burnUpgraded;)
    SPIRE_SNAPSHOT("Hexaghost.fireTackleCount", scalar, 2)
    SPIRE_SNAPSHOT("Hexaghost.fireTackleDmg", scalar, monster.damage[0])
    SPIRE_SNAPSHOT("Hexaghost.infernoDmg", scalar, monster.damage[3])
    SPIRE_SNAPSHOT("Hexaghost.infernoHits", scalar, 6)
    SPIRE_SNAPSHOT("Hexaghost.searBurnCount", scalar, game.run.ascension >= 19 ? 2 : 1)
    SPIRE_SNAPSHOT("Hexaghost.searDmg", scalar, 6)
    SPIRE_SNAPSHOT("Hexaghost.strAmount", scalar, game.run.ascension >= 19 ? 3 : 2)
    SPIRE_SNAPSHOT("Hexaghost.strengthenBlockAmt", scalar, 12)
    SPIRE_FN static constexpr MonsterData data() { return {264, 264, {6, 6, -1, 3}, 0, 0, 4, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 4) { stats.damage[0] = 5; stats.damage[3] = 2; }
        stats.hp_min = stats.hp_max = g.run.ascension >= 9 ? 264 : 250;
        return make_monster(g, MonsterId::Hexaghost, stats, hp_override);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        Monster& m = g.monsters[owner];
        auto& state = m.state.Hexaghost;
        if (!state.activated) {
            state.activated = true;
            set_monster_move(g, owner, 5, Intent::Unknown);
        } else switch (state.orbActiveCount) {
        case 0: case 2: case 5: set_monster_move(g, owner, 4, Intent::AttackDebuff, m.damage[1]); break;
        case 1: case 4: set_monster_move(g, owner, 2, Intent::Attack, m.damage[0], 2); break;
        case 3: set_monster_move(g, owner, 3, Intent::DefendBuff); break;
        case 6: set_monster_move(g, owner, 6, Intent::AttackDebuff, m.damage[3], 6); break;
        }
    }
    SPIRE_FN static void changeState(Game& g, int owner, const char* name) {
        Monster& m = g.monsters[owner];
        auto& state = m.state.Hexaghost;
        if (same_id(name, "Activate")) state.orbActiveCount = 6;
        else if (same_id(name, "Deactivate")) state.orbActiveCount = 0;
        else if (same_id(name, "Activate Orb")) {
            ++state.orbActiveCount;
            if (state.orbActiveCount == 6) set_monster_move(g, owner, 6, Intent::AttackDebuff, m.damage[3], 6);
        }
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 5) {
            add_bottom(g, actions::ChangeState::make(g, owner, "Activate"));
            m.damage[2] = g.player.creature.hp / 12 + 1;
            monster_applyPowers(g, owner);
            set_monster_move(g, owner, 1, Intent::Attack, m.damage[2], 6);
            return;
        }
        if (m.move == 1 || m.move == 6) {
            for (int i = 0; i < 6; ++i) queue_damage(g, owner, player_target, m.damage[m.move == 1 ? 2 : 3]);
            if (m.move == 6) {
                Effect e{}; e.kind = EffectKind::BurnIncrease; add_bottom(g, e);
                m.state.Hexaghost.burnUpgraded = true;
            }
            add_bottom(g, actions::ChangeState::make(g, owner, "Deactivate"));
        } else {
            if (m.move == 2) {
                queue_damage(g, owner, player_target, m.damage[0]);
                queue_damage(g, owner, player_target, m.damage[0]);
            } else if (m.move == 4) {
                queue_damage(g, owner, player_target, m.damage[1]);
                queue_discard_card(g, CardId::Burn, g.run.ascension >= 19 ? 2 : 1, m.state.Hexaghost.burnUpgraded ? 1 : 0);
            } else if (m.move == 3) {
                queue_block(g, owner, 12);
                queue_power(g, owner, owner, PowerId::Strength, g.run.ascension >= 19 ? 3 : 2);
            }
            add_bottom(g, actions::ChangeState::make(g, owner, "Activate Orb"));
        }
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        monster_die_base(g, owner, trigger_relics);
        monster_onBossVictoryLogic(g, owner);
    }
};
}
