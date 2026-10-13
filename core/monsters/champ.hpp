#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Champ : MonsterHooks {
    SPIRE_STATE(Champ, int numTurns; int forgeTimes; int16_t slashDmg; int16_t executeDmg; int16_t slapDmg; int16_t blockAmt; int16_t strAmt; int16_t forgeAmt; int16_t forgeThreshold; bool thresholdReached; bool firstTurn;)
    SPIRE_FN static constexpr MonsterData data() { return {440, 440, {18, 10, 14}, 20, 0, 3, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 9) stats.hp_min = stats.hp_max = 420;
        if (g.run.ascension < 4) { stats.damage[0] = 16; stats.damage[2] = 12; }
        Monster m = make_monster(g, MonsterId::Champ, stats, hp_override);
        position_monster(m, -90.0f);
        auto& s = m.state.Champ;
        s.slashDmg = m.damage[0]; s.executeDmg = m.damage[1]; s.slapDmg = m.damage[2];
        s.strAmt = g.run.ascension >= 19 ? 4 : g.run.ascension >= 4 ? 3 : 2;
        s.forgeAmt = g.run.ascension >= 19 ? 7 : g.run.ascension >= 9 ? 6 : 5;
        s.blockAmt = g.run.ascension >= 19 ? 20 : g.run.ascension >= 9 ? 18 : 15;
        s.forgeThreshold = 2; s.firstTurn = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner]; auto& s = m.state.Champ;
        ++s.numTurns;
        if (m.creature.hp < m.creature.max_hp / 2 && !s.thresholdReached) {
            s.thresholdReached = true; set_monster_move(g, owner, 7, Intent::Buff);
        } else if (!last_move(m, 3) && !(m.move_history.size >= 2 && m.move_history[m.move_history.size - 2] == 3) && s.thresholdReached)
            { add_top(g, actions::Talk::make(owner)); set_monster_move(g, owner, 3, Intent::Attack, m.damage[1], 2); }
        else if (s.numTurns == 4 && !s.thresholdReached) { set_monster_move(g, owner, 6, Intent::Debuff); s.numTurns = 0; }
        else if (!last_move(m, 2) && s.forgeTimes < s.forgeThreshold && num <= (g.run.ascension >= 19 ? 30 : 15)) {
            ++s.forgeTimes; set_monster_move(g, owner, 2, Intent::DefendBuff);
        } else if (!last_move(m, 5) && !last_move(m, 2) && num <= 30) set_monster_move(g, owner, 5, Intent::Buff);
        else if (!last_move(m, 4) && num <= 55) set_monster_move(g, owner, 4, Intent::AttackDebuff, m.damage[2]);
        else if (!last_move(m, 1)) set_monster_move(g, owner, 1, Intent::Attack, m.damage[0]);
        else set_monster_move(g, owner, 4, Intent::AttackDebuff, m.damage[2]);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner]; auto& s = m.state.Champ;
        if (s.firstTurn) {
            s.firstTurn = false;
            if (has_relic(g, "Champion Belt")) add_bottom(g, actions::Talk::make(owner));
        }
        if (m.move == 7) {
            Effect sfx{}; sfx.kind = EffectKind::SFX; add_bottom(g, sfx); add_bottom(g, actions::Shout::make(owner));
            for (int i = 0; i < 3; ++i) add_bottom(g, actions::VFX::make(owner));
            Effect e{}; e.kind = EffectKind::RemoveDebuffsAction; e.target = owner; add_bottom(g, e);
            e = {}; e.kind = EffectKind::RemovePower; e.source = e.target = owner; e.power = PowerId::GainStrength; add_bottom(g, e);
            queue_power(g, owner, owner, PowerId::Strength, s.strAmt * 3);
        } else if (m.move == 1) {
            add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK")); add_bottom(g, actions::VFX::make());
            queue_damage(g, owner, player_target, m.damage[0]);
        } else if (m.move == 2) { queue_block(g, owner, s.blockAmt); queue_power(g, owner, owner, PowerId::Metallicize, s.forgeAmt); }
        else if (m.move == 3) {
            add_bottom(g, actions::AnimateJump::make(owner));
            for (int i = 0; i < 2; ++i) {
                add_bottom(g, actions::VFX::make());
                queue_damage(g, owner, player_target, m.damage[1]);
            }
        } else if (m.move == 4) {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e); add_bottom(g, actions::AnimateFastAttack::make(owner));
            queue_damage(g, owner, player_target, m.damage[2]);
            queue_power(g, owner, player_target, PowerId::Frail, 2, true);
            queue_power(g, owner, player_target, PowerId::Vulnerable, 2, true);
        } else if (m.move == 5) queue_power(g, owner, owner, PowerId::Strength, s.strAmt);
        else if (m.move == 6) { Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e); add_bottom(g, actions::Talk::make(owner)); queue_power(g, owner, player_target, PowerId::Weak, 2, true); queue_power(g, owner, player_target, PowerId::Vulnerable, 2, true); }
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics) {
        monster_die_base(g, owner, trigger_relics); monster_onBossVictoryLogic(g, owner);
    }
};
}
