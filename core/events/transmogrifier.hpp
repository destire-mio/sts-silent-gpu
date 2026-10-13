#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Transmogrifier : EventHooks {
    SPIRE_STATE(Transmogrifier, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.Transmogrifier", true}; }
    SPIRE_SNAPSHOT("Transmogrifier.screen", scalar, event.state.Transmogrifier.screen ? "COMPLETE" : "INTRO")
    static SPIRE_FN void enter(Game& g) { event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Transmogrifier;
        if (s.screen) { open_map(g); return; }
        s.screen = 1; event_options(g, 1);
        if (option == 0) { event_purge_cards(g); open_master_selection(g, 1, false); g.grid_for_transform = true; }
    }
    static SPIRE_FN void update(Game& g) {
        if (g.selected_cards.empty()) return;
        const int chosen = g.selected_cards[0];
        const Card card = g.run.master_deck[chosen];
        remove_master_card(g, chosen);
        queue_card_obtain(g, transform_card(g, card, false, g.rng.misc));
        g.selected_cards.clear();
    }
};
}
