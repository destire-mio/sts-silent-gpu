#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct TransmutationAction : ActionHooks {
    SPIRE_SNAPSHOT("TransmutationAction.p", creature, effect.payload.state.TransmutationAction.p)
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(TransmutationAction, int energy_on_use; int p; bool upgraded; bool free_to_play_once;)
    SPIRE_FN static Effect construct(int player, bool upgraded, bool free, int energy) {
        Effect e{};
        e.kind = EffectKind::TransmutationAction;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.TransmutationAction.p = player;
        e.payload.state.TransmutationAction.upgraded = upgraded;
        e.payload.state.TransmutationAction.free_to_play_once = free;
        e.payload.state.TransmutationAction.energy_on_use = energy;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        const auto& state = e.payload.state.TransmutationAction;
        int effect = state.energy_on_use == -1 ? g.player.energy : state.energy_on_use;
        if (has_relic(g, "Chemical X")) effect += 2;
        if (effect > 0) {
            for (int i = 0; i < effect; ++i) {
                const int card = create_card(g, make_card(card_id(random_card_in_combat(g, -1, true))));
                if (state.upgraded) card_upgrade(g.cards[card]);
                if (g.cards[card].cost_for_turn >= 0) g.cards[card].cost_for_turn = 0;
                Effect copy{};
                copy.kind = EffectKind::MakeTempCardInHand;
                copy.card = card;
                copy.amount = 1;
                add_bottom(g, copy);
            }
            if (!state.free_to_play_once) g.player.energy = 0;
        }
        e.done = true;
    }
};
}
