#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct Looter : MonsterHooks {
    SPIRE_SNAPSHOT("Looter.escapeDef", scalar, units::monsters::Looter::data().block)
    SPIRE_SNAPSHOT("Looter.goldAmt", scalar, units::monsters::Looter::gold_amount(game))
    SPIRE_SNAPSHOT("Looter.lungeDmg", scalar, monster_base_damage(graph.game, monster, 1))
    SPIRE_SNAPSHOT("Looter.swipeDmg", scalar, monster_base_damage(graph.game, monster, 0))
    SPIRE_STATE(Looter, int32_t stolenGold; uint8_t slashCount;)
    SPIRE_FN static constexpr MonsterData data() { return {46, 50, {11, 14}, 6, 20, 2}; }
    SPIRE_FN static int gold_amount(const Game& g) { return g.run.ascension < 17 ? 15 : data().magic; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        MonsterData stats = data();
        if (g.run.ascension < 7) { stats.hp_min = 44; stats.hp_max = 48; }
        if (g.run.ascension < 2) { stats.damage[0] = 10; stats.damage[1] = 12; }
        Monster m = make_monster(g, MonsterId::Looter, stats, hp_override);
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        set_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
    }
    SPIRE_FN static void takeTurn(Game& g, int index) {
        Monster& m = g.monsters[index];
        if (m.move == 1 || m.move == 4) {
            const int move = m.move;
            // Looter.takeTurn consumes aiRng for its first Mug speech even without rendering.
            if (move == 1 && m.state.Looter.slashCount == 0) g.rng.ai.randomBoolean(0.6f);
            Effect steal{};
            steal.kind = EffectKind::StealGold;
            steal.target = index;
            steal.amount = gold_amount(g);
            add_bottom(g, steal);
            queue_damage(g, index, player_target, monster_base_damage(g, m, move == 1 ? 0 : 1), DamageType::Normal, gold_amount(g));
            ++m.state.Looter.slashCount;
            if (move == 4) set_monster_move(g, index, 2, Intent::Defend);
            else if (m.state.Looter.slashCount == 2) {
                if (g.rng.ai.randomBoolean(0.5f)) set_monster_move(g, index, 2, Intent::Defend);
                else queue_monster_move(g, index, 4, Intent::Attack, monster_base_damage(g, m, 1));
            } else queue_monster_move(g, index, 1, Intent::Attack, monster_base_damage(g, m, 0));
        } else if (m.move == 2) {
            queue_block(g, index, data().block);
            queue_monster_move(g, index, 3, Intent::Escape);
        } else {
            g.mugged = true;
            Effect e{};
            e.kind = EffectKind::Escape;
            e.target = index;
            add_bottom(g, e);
            queue_monster_move(g, index, 3, Intent::Escape);
        }
    }
    SPIRE_FN static void die(Game& g, int owner, bool trigger_relics = true) {
        if (g.monsters[owner].state.Looter.stolenGold > 0) add_stolen_gold_reward(g, g.monsters[owner].state.Looter.stolenGold);
        monster_die_base(g, owner, trigger_relics);
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int index) {
        queue_power(g, index, index, PowerId::Thievery, gold_amount(g));
    }
};
}
