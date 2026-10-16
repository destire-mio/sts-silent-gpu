#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::monsters {
struct AwakenedOne : MonsterHooks {
    SPIRE_STATE(AwakenedOne, bool form1; bool firstTurn;)
    SPIRE_FN static constexpr MonsterData data() { return {320, 320, {20, 6, 40, 18, 10}, 0, 0, 5, EnemyType::Boss}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        stats.hp_min = stats.hp_max = g.run.ascension >= 9 ? 320 : 300;
        Monster m = make_monster(g, MonsterId::AwakenedOne, stats, hp_override);
        m.state.AwakenedOne.form1 = m.state.AwakenedOne.firstTurn = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        g.cannot_lose = true;
        queue_power(g, owner, owner, PowerId::RegenerateMonster, g.run.ascension >= 19 ? 15 : 10);
        queue_power(g, owner, owner, PowerId::Curiosity, g.run.ascension >= 19 ? 2 : 1);
        queue_power(g, owner, owner, PowerId::Unawakened, -1);
        if (g.run.ascension >= 4) queue_power(g, owner, owner, PowerId::Strength, 2);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        auto& state = m.state.AwakenedOne;
        if (state.form1) {
            if (state.firstTurn) { state.firstTurn = false; set_monster_move(g, owner, 1, Intent::Attack, 20); }
            else if ((num < 25 && !last_move(g, m, 2)) || (num >= 25 && last_two_moves(g, m, 1))) set_monster_move(g, owner, 2, Intent::Attack, 6, 4);
            else set_monster_move(g, owner, 1, Intent::Attack, 20);
        } else {
            if (state.firstTurn) set_monster_move(g, owner, 5, Intent::Attack, 40);
            else if ((num < 50 && !last_two_moves(g, m, 6)) || (num >= 50 && last_two_moves(g, m, 8))) set_monster_move(g, owner, 6, Intent::AttackDebuff, 18);
            else set_monster_move(g, owner, 8, Intent::Attack, 10, 3);
        }
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 1 || m.move == 5 || m.move == 6) {
            add_bottom(g, actions::ChangeState::make(g, owner, m.move == 1 ? "ATTACK_1" : "ATTACK_2"));
            if (m.move == 5) m.state.AwakenedOne.firstTurn = false;
            queue_monster_damage(g, owner, player_target, m.move == 1 ? 0 : m.move == 5 ? 2 : 3);
            if (m.move == 6) {
                Effect e{}; e.kind = EffectKind::MakeTempCardInDrawPile; action_initialize_state(e);
                e.source = e.target = no_target;
                e.card = create_card(g, make_card(CardId::VoidCard));
                e.amount = 1;
                e.payload.state.MakeTempCardInDrawPile.random_spot = true;
                add_bottom(g, e);
            }
        } else if (m.move == 2 || m.move == 8) {
            const int count = m.move == 2 ? 4 : 3;
            for (int i = 0; i < count; ++i) queue_monster_damage(g, owner, player_target, m.move == 2 ? 1 : 4);
        } else if (m.move == 3) add_bottom(g, actions::ChangeState::make(g, owner, "REBIRTH"));
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void changeState(Game& g, int owner, const char* key) {
        if (same_text(key, "REBIRTH")) {
            Creature& c = g.monsters[owner].creature;
            c.max_hp = g.run.ascension >= 9 ? 320 : 300;
            c.half_dead = false;
            Effect e{}; e.kind = EffectKind::Heal; e.source = e.target = owner; e.amount = c.max_hp; add_bottom(g, e);
            e = Effect{}; e.kind = EffectKind::CanLose; add_bottom(g, e);
        }
    }
    SPIRE_FN static void damage(Game& g, int owner, const DamageInfo& info) {
        monster_damage_base(g, owner, info);
        Monster& m = g.monsters[owner];
        Creature& c = m.creature;
        if (c.hp <= 0 && !c.half_dead) {
            if (g.cannot_lose) c.half_dead = true;
            for (int p = next_power_onDeath(g, c.powers.head); p != 255; p = next_power_onDeath(g, g.power_objects[p].next)) power_onDeath(g, g.power_objects[p], owner);
            for (int i = next_relic_onMonsterDeath(g, 0); i < g.run.relics.size; i = next_relic_onMonsterDeath(g, i + 1)) relic_onMonsterDeath(g, i, owner);
            Effect e{}; e.kind = EffectKind::ClearCardQueue; add_top(g, e);
            for (int i = 0; i < c.powers.size;) {
                const Power& p = c.powers.at(g.power_objects, i);
                // AwakenedOne.damage reads the instance type; negative Strength is a debuff.
                if (power_isDebuff(p, owner) || p.id == PowerId::Curiosity || p.id == PowerId::Unawakened || (p.id == PowerId::GainStrength)) c.powers.erase(g.power_objects, i);
                else ++i;
            }
            set_monster_move(g, owner, 3, Intent::Unknown);
            monster_createIntent(g, owner);
            queue_monster_move(g, owner, 3, Intent::Unknown);
            monster_applyPowers(g, owner);
            m.state.AwakenedOne.firstTurn = true;
            m.state.AwakenedOne.form1 = false;
        }
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        if (!g.cannot_lose) {
            monster_die_base(g, owner, trigger_relics);
            for (int i = 0; i < g.monster_order.size; ++i) {
                const int id = g.monster_order[i];
                if (!g.monsters[id].creature.dying && g.monsters[id].id == MonsterId::Cultist) {
                    Effect e{}; e.kind = EffectKind::Escape; e.target = id; add_bottom(g, e);
                }
            }
            monster_onBossVictoryLogic(g, owner);
            monster_onFinalBossVictoryLogic(g, owner);
        }
    }
};
}
