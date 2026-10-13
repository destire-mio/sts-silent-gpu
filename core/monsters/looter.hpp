#pragma once
#include "base.hpp"

namespace spire::units::monsters {
struct Looter : MonsterHooks {
    SPIRE_STATE(Looter, int32_t stolenGold; int16_t escapeDef; int16_t goldAmt; int16_t lungeDmg; int16_t slashCount; int16_t swipeDmg;)
    SPIRE_FN static constexpr MonsterData data() { return {46, 50, {11, 14}, 6, 20, 2}; }
    SPIRE_FN static Monster construct(Game& g, int hp_override) {
        Monster m = make_monster(g, MonsterId::Looter, data(), hp_override);
        m.state.Looter.escapeDef = m.block_amount;
        m.state.Looter.goldAmt = m.magic;
        m.state.Looter.lungeDmg = m.damage[1];
        m.state.Looter.swipeDmg = m.damage[0];
        return m;
    }
    SPIRE_FN static void getMove(Game& g, int index, int num) {
        Monster& m = g.monsters[index];
        (void)num;
        set_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
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
            steal.amount = m.state.Looter.goldAmt;
            add_bottom(g, steal);
            queue_damage(g, index, player_target, m.damage[move == 1 ? 0 : 1], DamageType::Normal, m.state.Looter.goldAmt);
            ++m.state.Looter.slashCount;
            if (move == 4) set_monster_move(g, index, 2, Intent::Defend);
            else if (m.state.Looter.slashCount == 2) {
                if (g.rng.ai.randomBoolean(0.5f)) set_monster_move(g, index, 2, Intent::Defend);
                else queue_monster_move(g, index, 4, Intent::Attack, m.damage[1]);
            } else queue_monster_move(g, index, 1, Intent::Attack, m.damage[0]);
        } else if (m.move == 2) {
            queue_block(g, index, m.state.Looter.escapeDef);
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
        g.room_stolen_gold += g.monsters[owner].state.Looter.stolenGold;
        monster_die_base(g, owner, trigger_relics);
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int index) {
        queue_power(g, index, index, PowerId::Thievery, g.monsters[index].state.Looter.goldAmt);
    }
};
}
