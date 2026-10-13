#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct GoldenWing : EventHooks {
    SPIRE_STATE(GoldenWing, int goldAmount; uint8_t screen; bool canAttack; bool purgeResult;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.GoldenWing", true}; }
    SPIRE_SNAPSHOT("GoldenWing.damage", scalar, 7)
    SPIRE_SNAPSHOT("GoldenWing.screen", scalar, event.state.GoldenWing.screen == 0 ? "INTRO" : event.state.GoldenWing.screen == 1 ? "PURGE" : "MAP")
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.GoldenWing;
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            const auto& card = g.run.master_deck[i];
            if (card_data(card.id).type == CardType::Attack && card.base_damage >= 10) s.canAttack = true;
        }
        event_options(g, 3); g.image_dialog[1] = !s.canAttack;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.GoldenWing;
        if (s.screen == 0) {
            if (option == 0) { event_damage(g, 7, player_target); s.screen = 1; }
            else {
                if (option == 1) { s.goldAmount = g.rng.misc.random(50, 80); gain_gold(g, s.goldAmount); }
                s.screen = 2;
            }
            event_options(g, 1);
        } else if (s.screen == 1) {
            event_purge_cards(g); open_master_selection(g, 1, true);
            s.purgeResult = true; s.screen = 2;
        } else open_map(g);
    }
    static SPIRE_FN void update(Game& g) {
        auto& s = g.event.state.GoldenWing;
        if (!s.purgeResult || g.selected_cards.empty()) return;
        remove_master_card(g, g.selected_cards[0]); g.selected_cards.clear(); s.purgeResult = false;
    }
};
}
