#pragma once
#include "core/hooks.hpp"
#include "core/actions/unlimbo.hpp"
#include "core/actions/new_queue_card.hpp"

namespace spire::units::actions {
struct PlayTopCardAction : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_STATE(PlayTopCardAction, bool exhaust_cards;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.payload.state.PlayTopCardAction.exhaust_cards = e.flag;
        e.source = player_target;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (g.draw.size + g.discard.size == 0) { e.done = true; return; }
        if (g.draw.empty()) {
            Effect again = e;
            again.done = false;
            add_top(g, again);
            Effect shuffle{};
            shuffle.kind = EffectKind::Shuffle;
            add_top(g, shuffle);
            e.done = true;
            return;
        }
        const int card = g.draw.pop_back();
        // PlayTopCardAction.update removes the card's Soul before applying powers.
        g.pending_card_moves[card / 64] &= ~(uint64_t{1} << (card % 64));
        g.cards[card].exhaust_on_use_once = e.payload.state.PlayTopCardAction.exhaust_cards;
        g.limbo.push_back(card);
        card_applyPowers(g, card);
        add_top(g, NewQueueCard::construct(card, e.target, false, true));
        add_top(g, Unlimbo::construct(card));
        e.done = true;
    }
};
}
