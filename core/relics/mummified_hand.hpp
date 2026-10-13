#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct MummifiedHand : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void onUseCard(Game& g, int relic, int card, Effect&) {
        if (card_data(g.cards[card].id).type != CardType::Power) return;
        add_top(g, relic_above_creature(player_target, relic));
        Pile candidates{};
        for (int i = 0; i < g.hand.size; ++i) {
            const Card& c = g.cards[g.hand[i]];
            if (c.cost > 0 && c.cost_for_turn > 0 && !c.free_to_play_once) candidates.push_back(g.hand[i]);
        }
        for (int i = 0; i < g.manager.card_queue.size; ++i) {
            const int queued = g.manager.card_queue[i].card;
            if (queued >= 0) remove_from_pile(candidates, queued);
        }
        if (!candidates.empty()) {
            Card& c = g.cards[candidates[g.rng.card_random.random(0, candidates.size - 1)]];
            c.cost_for_turn = 0;
        }
    }
};
}
