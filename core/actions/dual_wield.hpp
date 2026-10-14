#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DualWield : ActionHooks {
    SPIRE_WAITING_RULE("DualWieldAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr ActionData data() { return {false, "DRAW"}; }
    SPIRE_STATE(DualWield, uint8_t cannotDuplicate[10]; uint8_t count; bool selecting;)
    SPIRE_SNAPSHOT("DualWieldAction.p", player)
    SPIRE_SNAPSHOT("DualWieldAction.dupeAmount", scalar, effect.amount)
    SPIRE_SNAPSHOT("DualWieldAction.cannotDuplicate", card_array, effect.payload.state.DualWield.cannotDuplicate, effect.payload.state.DualWield.count)
    static SPIRE_FN bool eligible(const Game& g, int card) {
        const CardType type = card_data(g.cards[card].id).type;
        return type == CardType::Attack || type == CardType::Power;
    }
    static SPIRE_FN void duplicate(Game& g, int card) {
        Effect a{};
        a.kind = EffectKind::MakeTempCardInHand;
        action_initialize_state(a);
        a.source = a.target = no_target;
        a.card = create_card(g, card_makeStatEquivalentCopy(g, card));
        a.amount = 1;
        add_top(g, a);
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (!e.payload.state.DualWield.selecting) {
            auto &state = e.payload.state.DualWield;
            state.count = 0;
            for (int i = 0; i < g.hand.size; ++i)
                if (!eligible(g, g.hand[i]))
                    state.cannotDuplicate[state.count++] = g.hand[i];
            if (state.count == g.hand.size)
                return;
            if (g.hand.size - state.count == 1) {
                for (int i = 0; i < g.hand.size; ++i)
                    if (eligible(g, g.hand[i]))
                        for (int j = 0; j < e.amount; ++j)
                            duplicate(g, g.hand[i]);
                return;
            }
            for (int i = 0; i < state.count; ++i)
                remove_from_pile(g.hand, state.cannotDuplicate[i]);
            open_hand_selection(g, 1);
            state.selecting = true;
            return;
        }
        if (!g.selected_cards_retrieved) {
            for (int i = 0; i < g.selected_cards.size; ++i)
                for (int j = 0; j <= e.amount; ++j)
                    duplicate(g, g.selected_cards[i]);
            for (int i = 0; i < e.payload.state.DualWield.count; ++i)
                g.hand.push_back(e.payload.state.DualWield.cannotDuplicate[i]);
            refresh_hand(g);
            g.selected_cards_retrieved = true;
            g.selected_cards.clear();
        }
    }
};
}
