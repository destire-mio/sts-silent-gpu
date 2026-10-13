#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct GamblingChipAction : ActionHooks {
    SPIRE_SNAPSHOT("GamblingChipAction.p", scalar, nullptr)
    static SPIRE_FN constexpr ActionData data() { return {false, "CARD_MANIPULATION"}; }
    SPIRE_STATE(GamblingChipAction, bool notchip; bool selecting;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.state.GamblingChipAction.notchip = e.flag;
        e.target = player_target;
        e.amount = -1;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (!e.state.GamblingChipAction.selecting) {
            open_hand_selection(g, 99, true, true);
            e.state.GamblingChipAction.selecting = true;
            return;
        }
        if (!g.selected_cards_retrieved) {
            if (!g.selected_cards.empty()) {
                Effect draw{};
                draw.kind = EffectKind::Draw;
                // GamblingChipAction.update passes its never-initialized p to DrawCardAction.
                draw.source = no_target;
                draw.amount = g.selected_cards.size;
                add_top(g, draw);
                for (int i = 0; i < g.selected_cards.size; ++i) {
                    const int card = g.selected_cards[i];
                    move_to_discard(g, card);
                    increment_discard(g, false);
                    card_triggerOnManualDiscard(g, card);
                }
            }
            g.selected_cards_retrieved = true;
        }
        e.done = true;
    }
};
}
