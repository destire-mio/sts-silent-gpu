#pragma once
#include "base.hpp"
#include "../actions/intent_flash.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::monsters {
struct Darkling : MonsterHooks {
    SPIRE_SNAPSHOT("Darkling.chompDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_SNAPSHOT("Darkling.nipDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_STATE(Darkling, bool firstMove;)
    SPIRE_FN static constexpr MonsterData data() { return {50, 59, {9, 0}, 12, 0, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 48; stats.hp_max = 56; }
        if (g.run.ascension < 2) stats.damage[0] = 8;
        Monster m = make_monster(g, MonsterId::Darkling, stats, hp_override);
        monster_base_damage(g, m, 1) = g.run.ascension >= 2 ? g.rng.monster_hp.random(9, 13) : g.rng.monster_hp.random(7, 11);
        monster_output_damage(g, m, 1) = monster_base_damage(g, m, 1);
        m.state.Darkling.firstMove = true;
        return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        g.cannot_lose = true;
        queue_power(g, owner, owner, PowerId::Regrow, -1);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        Monster& m = g.monsters[owner];
        if (m.creature.half_dead) { set_monster_move(g, owner, 5, Intent::Buff); return; }
        if (m.state.Darkling.firstMove) {
            if (num < 50) set_monster_move(g, owner, 2, g.run.ascension >= 17 ? Intent::DefendBuff : Intent::Defend);
            else set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, m, 1));
            m.state.Darkling.firstMove = false;
            return;
        }
        int position = 0;
        for (int i = 0; i < g.monster_order.size; ++i) if (g.monster_order[i] == owner) position = i;
        for (;;) {
            if (num < 40) {
                if (!last_move(g, m, 1) && position % 2 == 0) { set_monster_move(g, owner, 1, Intent::Attack, monster_base_damage(g, m, 0), 2); return; }
                num = g.rng.ai.random(40, 99);
            } else if (num < 70) {
                if (!last_move(g, m, 2)) set_monster_move(g, owner, 2, g.run.ascension >= 17 ? Intent::DefendBuff : Intent::Defend);
                else set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, m, 1));
                return;
            } else if (!last_two_moves(g, m, 3)) { set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, m, 1)); return; }
            else num = g.rng.ai.random(0, 99);
        }
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        const Monster& m = g.monsters[owner];
        if (m.move == 1) { add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK")); queue_monster_damage(g, owner, player_target, 0); queue_monster_damage(g, owner, player_target, 0); }
        else if (m.move == 2) { queue_block(g, owner, 12); if (g.run.ascension >= 17) queue_power(g, owner, owner, PowerId::Strength, 2); }
        else if (m.move == 3) { add_bottom(g, actions::AnimateFastAttack::make(owner)); queue_monster_damage(g, owner, player_target, 1); }
        else if (m.move == 4) add_bottom(g, actions::TextAboveCreature::make(owner));
        else if (m.move == 5) {
            Effect sfx{}; sfx.kind = EffectKind::SFX; add_bottom(g, sfx);
            Effect e{}; e.kind = EffectKind::Heal; e.source = e.target = owner; e.amount = m.creature.max_hp / 2; add_bottom(g, e);
            add_bottom(g, actions::ChangeState::make(g, owner, "REVIVE"));
            queue_power(g, owner, owner, PowerId::Regrow, 1);
            for (int i = next_relic_onSpawnMonster(g, 0); i < g.run.relics.size; i = next_relic_onSpawnMonster(g, i + 1)) relic_onSpawnMonster(g, i, owner);
        }
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void changeState(Game& g, int owner, const char* key) {
        if (same_text(key, "REVIVE")) g.monsters[owner].creature.half_dead = false;
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics) {
        if (!g.cannot_lose) monster_die_base(g, owner, trigger_relics);
    }
    SPIRE_FN static void damage(Game& g, int owner, const DamageInfo& info) {
        monster_damage_base(g, owner, info);
        Creature& c = g.monsters[owner].creature;
        if (c.hp <= 0 && !c.half_dead) {
            c.half_dead = true;
            for (int p = next_power_onDeath(g, c.powers.head); p != 255; p = next_power_onDeath(g, g.power_objects[p].next)) power_onDeath(g, g.power_objects[p], owner);
            for (int i = next_relic_onMonsterDeath(g, 0); i < g.run.relics.size; i = next_relic_onMonsterDeath(g, i + 1)) relic_onMonsterDeath(g, i, owner);
            c.powers.clear();
            bool all_dead = true;
            for (int i = 0; i < g.monster_order.size; ++i) {
                const Monster& other = g.monsters[g.monster_order[i]];
                if (other.id == MonsterId::Darkling && !other.creature.half_dead) all_dead = false;
            }
            if (!all_dead) {
                if (g.monsters[owner].move != 4) {
                    set_monster_move(g, owner, 4, Intent::Unknown);
                    monster_createIntent(g, owner);
                    queue_monster_move(g, owner, 4, Intent::Unknown);
                }
            } else {
                g.cannot_lose = false; c.half_dead = false;
                for (int i = 0; i < g.monster_order.size; ++i) monster_die(g, g.monster_order[i], true);
            }
        }
    }
};
}
