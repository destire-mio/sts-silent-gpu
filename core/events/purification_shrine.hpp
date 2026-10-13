#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct PurificationShrine : EventHooks {
    SPIRE_STATE(PurificationShrine, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.PurificationShrine", true}; }
    SPIRE_SNAPSHOT("PurificationShrine.screen", scalar, event.state.PurificationShrine.screen == 0 ? "INTRO" : "COMPLETE")
    static SPIRE_FN void enter(Game& g) { event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.PurificationShrine;
        if (s.screen == 0) {
            s.screen = 1; event_options(g, 1);
            if (option == 0) { event_purge_cards(g); open_master_selection(g, 1, true); }
        } else open_map(g);
    }
    static SPIRE_FN void update(Game& g) {
        if (g.selected_cards.empty()) return;
        remove_master_card(g, g.selected_cards[0]); g.selected_cards.clear();
    }
};
}
