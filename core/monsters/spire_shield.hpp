#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct SpireShield : MonsterHooks {
    SPIRE_STATE(SpireShield, int moveCount;)
    SPIRE_FN static constexpr MonsterData data() { return {125, 125, {14, 38}, 0, 0, 2, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 3) { d.damage[0] = 12; d.damage[1] = 34; }
        d.hp_min = d.hp_max = g.run.ascension >= 8 ? 125 : 110;
        Monster m = make_monster(g, MonsterId::SpireShield, d, hp);
        position_monster(m, -1000.0f); return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, player_target, PowerId::Surrounded, -1);
        queue_power(g, owner, owner, PowerId::Artifact, g.run.ascension >= 18 ? 2 : 1);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        (void)num; Monster& m = g.monsters[owner];
        const int phase = m.state.SpireShield.moveCount++ % 3;
        if (phase == 2) set_monster_move(g, owner, 3, Intent::AttackDefend, monster_base_damage(g, m, 1));
        else if (phase == 0 ? g.rng.ai.randomBoolean() : last_move(g, m, 1)) set_monster_move(g, owner, 2, Intent::Defend);
        else set_monster_move(g, owner, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 1) {
            queue_damage(g, owner, player_target, monster_base_damage(g, m, 0));
            queue_power(g, owner, player_target, PowerId::Strength, -1);
        } else if (m.move == 2) {
            for (int i = 0; i < g.monster_order.size; ++i) queue_block(g, g.monster_order[i], 30);
        } else {
            queue_damage(g, owner, player_target, monster_base_damage(g, m, 1));
            queue_block(g, owner, g.run.ascension >= 18 ? 99 : monster_output_damage(g, m, 1));
        }
        queue_monster_roll(g, owner);
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        monster_die_base(g, owner, trigger_relics);
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int target = g.monster_order[i];
            const Creature& c = g.monsters[target].creature;
            if (c.dead || c.dying) continue;
            if (has_power(g, g.player.creature, PowerId::Surrounded)) {
                g.player.creature.flip_horizontal = c.draw_x < g.player.creature.draw_x;
                Effect e{}; e.kind = EffectKind::RemovePower; e.source = e.target = player_target; e.power = PowerId::Surrounded; add_bottom(g, e);
            }
            if (has_power(g, c, PowerId::BackAttack)) {
                Effect e{}; e.kind = EffectKind::RemovePower; e.source = e.target = target; e.power = PowerId::BackAttack; add_bottom(g, e);
            }
        }
    }
};
}
