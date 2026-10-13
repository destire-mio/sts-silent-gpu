#pragma once
#include "base.hpp"
#include "../actions/change_state.hpp"

namespace spire::units::monsters {
struct SpireSpear : MonsterHooks {
    SPIRE_STATE(SpireSpear, int moveCount; int skewerCount;)
    SPIRE_FN static constexpr MonsterData data() { return {180, 180, {6, 10}, 0, 0, 2, EnemyType::Elite}; }
    SPIRE_FN static Monster construct(Game& g, int hp) {
        MonsterData d = data();
        if (g.run.ascension < 3) d.damage[0] = 5;
        d.hp_min = d.hp_max = g.run.ascension >= 8 ? 180 : 160;
        Monster m = make_monster(g, MonsterId::SpireSpear, d, hp);
        m.state.SpireSpear.skewerCount = g.run.ascension >= 3 ? 4 : 3;
        position_monster(m, 70.0f); return m;
    }
    SPIRE_FN static void usePreBattleAction(Game& g, int owner) {
        queue_power(g, owner, owner, PowerId::Artifact, g.run.ascension >= 18 ? 2 : 1);
    }
    SPIRE_FN static void getMove(Game& g, int owner, int num) {
        (void)num; Monster& m = g.monsters[owner];
        const int phase = m.state.SpireSpear.moveCount++ % 3;
        if (phase == 1) set_monster_move(g, owner, 3, Intent::Attack, monster_base_damage(g, m, 1), m.state.SpireSpear.skewerCount);
        else if (phase == 0 ? last_move(g, m, 1) : g.rng.ai.randomBoolean()) set_monster_move(g, owner, 2, Intent::Buff);
        else set_monster_move(g, owner, 1, Intent::AttackDebuff, monster_base_damage(g, m, 0), 2);
    }
    SPIRE_FN static void takeTurn(Game& g, int owner) {
        Monster& m = g.monsters[owner];
        if (m.move == 1) {
            for (int i = 0; i < 2; ++i) {
                add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
                queue_damage(g, owner, player_target, monster_base_damage(g, m, 0));
            }
            Effect e{}; e.kind = g.run.ascension >= 18 ? EffectKind::MakeTempCardInDrawPile : EffectKind::AddCardDiscard;
            action_initialize_state(e); e.card = g.cards.size; e.amount = 2;
            g.cards.push_back(make_card(CardId::Burn)); g.cards[e.card].uuid = g.next_card_uuid++;
            add_bottom(g, e);
        } else if (m.move == 2) {
            for (int i = 0; i < g.monster_order.size; ++i) queue_power(g, owner, g.monster_order[i], PowerId::Strength, 2);
        } else for (int i = 0; i < m.state.SpireSpear.skewerCount; ++i) {
            add_bottom(g, actions::ChangeState::make(g, owner, "ATTACK"));
            queue_damage(g, owner, player_target, monster_base_damage(g, m, 1));
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
