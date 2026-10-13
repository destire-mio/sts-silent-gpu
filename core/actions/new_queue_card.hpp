#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct NewQueueCard : ActionHooks {
    SPIRE_STATE(NewQueueCard, bool random_target; bool immediate_card; bool autoplay_card;)
    SPIRE_FN static Effect construct(int card = -1, int target = no_target, bool immediate = false, bool autoplay = false) {
        Effect e{};
        e.kind = EffectKind::NewQueueCard;
        action_initialize_state(e);
        e.source = no_target;
        e.target = target;
        e.card = card;
        e.state.NewQueueCard.immediate_card = immediate;
        e.state.NewQueueCard.autoplay_card = autoplay;
        return e;
    }
    SPIRE_FN static Effect construct_random(int card, bool random, bool immediate = false, bool autoplay = false) {
        Effect e = construct(card, no_target, immediate, autoplay);
        e.state.NewQueueCard.random_target = random;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        for (int i = 0; i < g.manager.card_queue.size; ++i)
            if (g.manager.card_queue[i].card == e.card) return;
        if (e.card < 0) { g.manager.card_queue.push_back({}); return; }
        if (!e.state.NewQueueCard.random_target && e.target == player_target) return;
        CardQueueItem item{};
        item.card = e.card;
        item.target = e.state.NewQueueCard.random_target ? no_target : e.target;
        item.energy_on_use = g.player.energy;
        item.autoplay_card = e.state.NewQueueCard.autoplay_card;
        item.random_target = e.state.NewQueueCard.random_target;
        add_card_queue_item(g, item, e.state.NewQueueCard.immediate_card);
    }
};
}
