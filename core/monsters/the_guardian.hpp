#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/lose_block.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct TheGuardian : MonsterHooks {
    SPIRE_STATE(TheGuardian, int dmgTaken; int16_t dmgThreshold; bool isOpen; bool closeUpTriggered;)
    SPIRE_SNAPSHOT("TheGuardian.DEFENSIVE_BLOCK", scalar, 20)
    SPIRE_SNAPSHOT("TheGuardian.VENT_DEBUFF", scalar, 2)
    SPIRE_SNAPSHOT("TheGuardian.blockAmount", scalar, 9)
    SPIRE_SNAPSHOT("TheGuardian.dmgThresholdIncrease", scalar, 10)
    SPIRE_SNAPSHOT("TheGuardian.fierceBashDamage", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("TheGuardian.rollDamage", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_SNAPSHOT("TheGuardian.thornsDamage", scalar, 3)
    SPIRE_SNAPSHOT("TheGuardian.twinSlamDamage", scalar, 8)
    SPIRE_SNAPSHOT("TheGuardian.whirlwindCount", scalar, 4)
    SPIRE_SNAPSHOT("TheGuardian.whirlwindDamage", scalar, 5)
    SPIRE_FN static constexpr MonsterData data() { return {250, 250, {36, 10, 5, 8}, 0, 0, 4, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 4) { stats.damage[0] = 32; stats.damage[1] = 9; }
        stats.hp_min = stats.hp_max = g.run.ascension >= 9 ? 250 : 240;
        Monster m = make_monster(g, MonsterId::TheGuardian, stats, hp_override);
        m.state.TheGuardian.dmgThreshold = g.run.ascension >= 19 ? 40 : g.run.ascension >= 9 ? 35 : 30;
        m.state.TheGuardian.isOpen = true;
        position_monster(m, -50.0f);
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::ModeShift, g.monsters[owner].state.TheGuardian.dmgThreshold);
        add_bottom(g, actions::ChangeState::make(g, owner, "Reset Threshold"));
    }
    SPIRE_FN static void getMove(Game& g, int owner, int) {
        if (g.monsters[owner].state.TheGuardian.isOpen) set_monster_move(g, owner, 6, Intent::Defend);
        else set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, g.monsters[owner], 1));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        switch (m.move) {
        case 1:
            queue_power(g, owner, owner, PowerId::SharpHide, g.run.ascension >= 19 ? 4 : 3);
            set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, m, 1));
            break;
        case 2:
            queue_monster_damage(g, owner, player_target, 0);
            set_monster_move(g, owner, 7, Intent::StrongDebuff);
            break;
        case 7:
            queue_power(g, owner, player_target, PowerId::Weak, 2, true);
            queue_power(g, owner, player_target, PowerId::Vulnerable, 2, true);
            set_monster_move(g, owner, 5, Intent::Attack, 5, 4);
            break;
        case 3:
            queue_monster_damage(g, owner, player_target, 1);
            set_monster_move(g, owner, 4, Intent::AttackBuff, 8, 2);
            break;
        case 4: {
            add_bottom(g, actions::ChangeState::make(g, owner, "Offensive Mode"));
            queue_monster_damage(g, owner, player_target, 3);
            queue_monster_damage(g, owner, player_target, 3);
            Effect e{}; e.kind = EffectKind::RemovePower; e.source = e.target = owner; e.power = PowerId::SharpHide; add_bottom(g, e);
            set_monster_move(g, owner, 5, Intent::Attack, 5, 4);
            break;
        }
        case 5: {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e);
            for (int i = 0; i < 4; ++i) {
                add_bottom(g, e);
                add_bottom(g, actions::VFX::make(owner));
                queue_monster_damage(g, owner, player_target, 2);
            }
            set_monster_move(g, owner, 6, Intent::Defend);
            break;
        }
        case 6:
            queue_block(g, owner, 9);
            set_monster_move(g, owner, 2, Intent::Attack, monster_base_damage(g, m, 0));
            break;
        }
    }
    SPIRE_FN static void changeState(Game& g, int owner, const char* name) {
        Monster& m = g.monsters[owner];
        auto& state = m.state.TheGuardian;
        if (same_id(name, "Defensive Mode")) {
            Effect e{}; e.kind = EffectKind::RemovePower; e.source = e.target = owner; e.power = PowerId::ModeShift; add_bottom(g, e);
            queue_block(g, owner, 20);
            state.dmgThreshold += 10;
            set_monster_move(g, owner, 1, Intent::Buff);
            monster_createIntent(g, owner);
            state.isOpen = false;
        } else if (same_id(name, "Offensive Mode")) {
            queue_power(g, owner, owner, PowerId::ModeShift, state.dmgThreshold);
            add_bottom(g, actions::ChangeState::make(g, owner, "Reset Threshold"));
            if (m.creature.block != 0) add_bottom(g, actions::LoseBlock::construct(owner, owner, m.creature.block));
            state.isOpen = true;
            state.closeUpTriggered = false;
        } else if (same_id(name, "Reset Threshold")) state.dmgTaken = 0;
    }
    SPIRE_FN static void damage(Game& g, int owner, const DamageInfo& info) {
        Monster& m = g.monsters[owner];
        const int hp = m.creature.hp;
        monster_damage_base(g, owner, info);
        auto& state = m.state.TheGuardian;
        if (state.isOpen && !state.closeUpTriggered && hp > m.creature.hp && !m.creature.dying) {
            const int amount = hp - m.creature.hp;
            state.dmgTaken += amount;
            const int power = find_power(g, m.creature, PowerId::ModeShift);
            if (power >= 0) m.creature.powers.at(g.power_objects, power).amount -= amount;
            if (state.dmgTaken >= state.dmgThreshold) {
                state.dmgTaken = 0;
                add_bottom(g, actions::VFX::make(owner));
                add_bottom(g, actions::ChangeState::make(g, owner, "Defensive Mode"));
                state.closeUpTriggered = true;
            }
        }
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        monster_die_base(g, owner, trigger_relics);
        monster_onBossVictoryLogic(g, owner);
    }
};
}
