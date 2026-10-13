#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::monsters {
struct Champ : MonsterHooks {
    SPIRE_STATE(Champ, uint16_t numTurns; uint8_t forgeTimes; bool thresholdReached; bool firstTurn;)
    SPIRE_SNAPSHOT("Champ.slashDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("Champ.executeDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_SNAPSHOT("Champ.slapDmg", scalar, monster_base_damage(graph.game, monster, 2))
    SPIRE_SNAPSHOT("Champ.strAmt", scalar, game.run.ascension >= 19 ? 4 : game.run.ascension >= 4 ? 3 : 2)
    SPIRE_SNAPSHOT("Champ.forgeAmt", scalar, game.run.ascension >= 19 ? 7 : game.run.ascension >= 9 ? 6 : 5)
    SPIRE_SNAPSHOT("Champ.blockAmt", scalar, game.run.ascension >= 19 ? 20 : game.run.ascension >= 9 ? 18 : 15)
    SPIRE_SNAPSHOT("Champ.forgeThreshold", scalar, 2)
    SPIRE_FN static constexpr MonsterData data() { return {440, 440, {18, 10, 14}, 20, 0, 3, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 9) stats.hp_min = stats.hp_max = 420;
        if (g.run.ascension < 4) { stats.damage[0] = 16; stats.damage[2] = 12; }
        Monster m = make_monster(g, MonsterId::Champ, stats, hp_override);
        position_monster(m, -90.0f);
        auto& s = m.state.Champ;

        s.firstTurn = true;
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner]; auto& s = m.state.Champ;
        ++s.numTurns;
        if (m.creature.hp < m.creature.max_hp / 2 && !s.thresholdReached) {
            s.thresholdReached = true; set_monster_move(g, owner, 7, Intent::Buff);
        } else if (!last_move(g, m, 3) && !(m.move_history.size >= 2 && g.move_histories.last(m.move_history, 1) == 3) && s.thresholdReached)
            { add_top(g, actions::Talk::make(owner)); set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, m, 1), 2); }
        else if (s.numTurns == 4 && !s.thresholdReached) { set_monster_move(g, owner, 6, Intent::Debuff); s.numTurns = 0; }
        else if (!last_move(g, m, 2) && s.forgeTimes < (2) && num <= (g.run.ascension >= 19 ? 30 : 15)) {
            ++s.forgeTimes; set_monster_move(g, owner, 2, Intent::DefendBuff);
        } else if (!last_move(g, m, 5) && !last_move(g, m, 2) && num <= 30) set_monster_move(g, owner, 5, Intent::Buff);
        else if (!last_move(g, m, 4) && num <= 55) set_monster_move(g, owner, 4, Intent::AttackDebuff, monster_base_damage(g, m, 2));
        else if (!last_move(g, m, 1)) set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0));
        else set_monster_move(g, owner, 4, Intent::AttackDebuff, monster_base_damage(g, m, 2));
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
            queue_power(g, owner, owner, PowerId::Strength, (g.run.ascension >= 19 ? 4 : g.run.ascension >= 4 ? 3 : 2) * 3);
        } else if (m.move == 1) {
            add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK")); add_bottom(g, actions::VFX::make());
            queue_damage(g, owner, player_target, monster_base_damage(g, m, 0));
        } else if (m.move == 2) { queue_block(g, owner, (g.run.ascension >= 19 ? 20 : g.run.ascension >= 9 ? 18 : 15)); queue_power(g, owner, owner, PowerId::Metallicize, (g.run.ascension >= 19 ? 7 : g.run.ascension >= 9 ? 6 : 5)); }
        else if (m.move == 3) {
            add_bottom(g, actions::AnimateJump::make(owner));
            for (int i = 0; i < 2; ++i) {
                add_bottom(g, actions::VFX::make());
                queue_damage(g, owner, player_target, monster_base_damage(g, m, 1));
            }
        } else if (m.move == 4) {
            Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e); add_bottom(g, actions::AnimateFastAttack::make(owner));
            queue_damage(g, owner, player_target, monster_base_damage(g, m, 2));
            queue_power(g, owner, player_target, PowerId::Frail, 2, true);
            queue_power(g, owner, player_target, PowerId::Vulnerable, 2, true);
        } else if (m.move == 5) queue_power(g, owner, owner, PowerId::Strength, (g.run.ascension >= 19 ? 4 : g.run.ascension >= 4 ? 3 : 2));
        else if (m.move == 6) { Effect e{}; e.kind = EffectKind::SFX; add_bottom(g, e); add_bottom(g, actions::Talk::make(owner)); queue_power(g, owner, player_target, PowerId::Weak, 2, true); queue_power(g, owner, player_target, PowerId::Vulnerable, 2, true); }
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics) {
        monster_die_base(g, owner, trigger_relics); monster_onBossVictoryLogic(g, owner);
    }
};
}
