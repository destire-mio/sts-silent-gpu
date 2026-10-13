#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Exhaust : ActionHooks {
    SPIRE_STATE(Exhaust, bool isRandom; bool anyNumber; bool canPickZero; bool selecting;)
    SPIRE_FN static Effect make(int amount, bool random, bool any_number, bool can_pick_zero) {
        Effect e{};
        e.kind = EffectKind::Exhaust;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.amount = amount;
        e.state.Exhaust.isRandom = random;
        e.state.Exhaust.anyNumber = any_number;
        e.state.Exhaust.canPickZero = can_pick_zero;
        return e;
    }
    SPIRE_FN static Effect make(int amount, bool can_pick_zero) { return make(amount, false, false, can_pick_zero); }
    SPIRE_FN static Effect make(int amount, bool random, bool any_number) { return make(amount, random, any_number, false); }
    SPIRE_FN static Effect make(bool random, bool any_number, bool can_pick_zero) { return make(99, random, any_number, can_pick_zero); }
    SPIRE_FN static Effect make(int target, int source, int amount, bool random, bool any_number = false, bool can_pick_zero = false) {
        Effect e = make(amount, random, any_number, can_pick_zero);
        e.source = source;
        e.target = target;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (e.state.Exhaust.selecting) {
            if (!g.selected_cards_retrieved) {
                for (int i = 0; i < g.selected_cards.size; ++i) exhaust_card(g, g.selected_cards[i]);
                g.selected_cards_retrieved = true;
            }
            return;
        }
        if (g.hand.empty()) return;
        if (!e.state.Exhaust.anyNumber && g.hand.size <= e.amount) {
            g.exhaust_action_num = e.amount = g.hand.size;
            const int count = g.hand.size;
            for (int i = 0; i < count; ++i) exhaust_card(g, g.hand[g.hand.size - 1]);
            return;
        }
        if (e.state.Exhaust.isRandom) {
            for (int i = 0; i < e.amount; ++i) {
                const int index = g.rng.card_random.random(g.hand.size - 1);
                exhaust_card(g, g.hand[index]);
            }
        } else {
            g.exhaust_action_num = e.amount;
            open_hand_selection(g, e.amount, e.state.Exhaust.anyNumber, e.state.Exhaust.canPickZero);
            e.state.Exhaust.selecting = true;
        }
    }
};
}
