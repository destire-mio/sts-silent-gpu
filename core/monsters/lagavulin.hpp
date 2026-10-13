#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::monsters {
struct Lagavulin : MonsterHooks {
    SPIRE_STATE(Lagavulin, int debuffTurnCount; int idleCount; bool asleep; bool isOut; bool isOutTriggered;)
    SPIRE_SNAPSHOT("Lagavulin.attackDmg", scalar, monster.damage[0])
    SPIRE_SNAPSHOT("Lagavulin.debuff", scalar, game.run.ascension >= 18 ? -2 : -1)
    SPIRE_FN static constexpr MonsterData data() { return {112, 115, {20, 0}, 0, 0, 1, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 8) { stats.hp_min = 109; stats.hp_max = 111; }
        if (g.run.ascension < 3) stats.damage[0] = 18;
        Monster m = make_monster(g, MonsterId::Lagavulin, stats, hp_override);
        m.state.Lagavulin.asleep = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        if (g.monsters[owner].state.Lagavulin.asleep) {
            queue_block(g, owner, 8);
            queue_power(g, owner, owner, PowerId::Metallicize, 8);
        } else set_monster_move(g, owner, 1, Intent::StrongDebuff);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        Monster& m = g.monsters[owner];
        if (!m.state.Lagavulin.isOut) set_monster_move(g, owner, 5, Intent::Sleep);
        else if (m.state.Lagavulin.debuffTurnCount < 2 && !last_two_moves(g, m, 3))
            set_monster_move(g, owner, 3, Intent::Attack, m.damage[0]);
        else set_monster_move(g, owner, 1, Intent::StrongDebuff);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        auto& state = m.state.Lagavulin;
        switch (m.move) {
        case 1:
            state.debuffTurnCount = 0;
            add_bottom(g, actions::ChangeState::make(g, owner, "DEBUFF"));
            queue_power(g, owner, player_target, PowerId::Dexterity, g.run.ascension >= 18 ? -2 : -1);
            queue_power(g, owner, player_target, PowerId::Strength, g.run.ascension >= 18 ? -2 : -1);
            queue_monster_roll(g, owner);
            break;
        case 3:
            ++state.debuffTurnCount;
            add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
            queue_damage(g, owner, player_target, m.damage[0]);
            queue_monster_roll(g, owner);
            break;
        case 5:
            ++state.idleCount;
            if (state.idleCount >= 3) {
                state.isOutTriggered = true;
                add_bottom(g, actions::ChangeState::make(g, owner, "OPEN"));
                queue_monster_move(g, owner, 3, Intent::Attack, m.damage[0]);
            } else set_monster_move(g, owner, 5, Intent::Sleep);
            if (state.idleCount == 1 || state.idleCount == 2) queue_monster_roll(g, owner);
            break;
        case 4: queue_monster_roll(g, owner); break;
        case 6:
            add_bottom(g, actions::ChangeState::make(g, owner, "OPEN"));
            set_monster_move(g, owner, 3, Intent::Attack, m.damage[0]);
            monster_createIntent(g, owner);
            state.isOutTriggered = true;
            queue_monster_roll(g, owner);
            break;
        }
    }
    SPIRE_FN static void changeState(Game& g, int owner, const char* name) {
        if (same_id(name, "OPEN") && !g.monsters[owner].creature.dying) {
            g.monsters[owner].state.Lagavulin.isOut = true;
            Effect e{};
            e.kind = EffectKind::ReducePower;
            e.target = e.source = owner;
            e.power = PowerId::Metallicize;
            e.amount = 8;
            add_bottom(g, e);
        }
    }
    SPIRE_FN static void damage(Game& g, int owner, const DamageInfo& info) {
        Monster& m = g.monsters[owner];
        const int hp = m.creature.hp;
        monster_damage_base(g, owner, info);
        if (m.creature.hp != hp && !m.state.Lagavulin.isOutTriggered) {
            set_monster_move(g, owner, 4, Intent::Stun);
            monster_createIntent(g, owner);
            m.state.Lagavulin.isOutTriggered = true;
            add_bottom(g, actions::ChangeState::make(g, owner, "OPEN"));
        }
    }
};
}
