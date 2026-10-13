#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct RitualDagger : ActionHooks {
    SPIRE_STATE(RitualDagger, int increaseAmount; uint32_t uuid;)
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_FN static Effect construct(int target, const DamageInfo& info, int increase, uint32_t uuid) {
        Effect e = info;
        e.kind = EffectKind::RitualDagger;
        action_initialize_state(e);
        e.target = target;
        e.state.RitualDagger.increaseAmount = increase;
        e.state.RitualDagger.uuid = uuid;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (e.target == no_target) return;
        creature_damage(g, e);
        const Creature& target = get_creature(g, e.target);
        if ((target.dying || target.hp <= 0) && !target.half_dead && !has_power(target, "Minion")) {
            for (int i = 0; i < g.run.master_deck.size; ++i) {
                Card& card = g.run.master_deck[i];
                if (card.uuid != e.state.RitualDagger.uuid) continue;
                card.misc += e.state.RitualDagger.increaseAmount;
                card_applyPowers(g, -i - 2);
                card.base_damage = card.misc;
            }
            bool included[card_capacity]{};
            if (g.player.card_in_use >= 0) included[g.player.card_in_use] = true;
            const Pile* piles[] = {&g.draw, &g.discard, &g.exhaust, &g.limbo, &g.hand};
            for (int p = 0; p < 5; ++p)
                for (int i = 0; i < piles[p]->size; ++i) included[(*piles[p])[i]] = true;
            for (int i = 0; i < g.cards.size; ++i) {
                Card& card = g.cards[i];
                if (!included[i] || card.uuid != e.state.RitualDagger.uuid) continue;
                card.misc += e.state.RitualDagger.increaseAmount;
                card_applyPowers(g, i);
                card.base_damage = card.misc;
            }
        }
        if (monsters_basically_dead(g)) clear_post_combat_actions(g);
    }
};
}
