#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct UpgradeShrine : EventHooks {
    SPIRE_STATE(UpgradeShrine, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.UpgradeShrine", true}; }
    SPIRE_SNAPSHOT("UpgradeShrine.screen", scalar, event.state.UpgradeShrine.screen ? "COMPLETE" : "INTRO")
    static SPIRE_FN void enter(Game& g) {
        event_options(g, 2); g.image_dialog[0] = true;
        for (int i = 0; i < g.run.master_deck.size; ++i)
            if (card_canUpgrade(g, -i - 2)) g.image_dialog[0] = false;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.UpgradeShrine;
        if (s.screen) { open_map(g); return; }
        s.screen = 1; g.room_phase = RoomPhase::Complete; event_options(g, 1);
        if (option == 0) {
            g.grid_cards.clear();
            for (int i = 0; i < g.run.master_deck.size; ++i)
                if (card_canUpgrade(g, -i - 2)) g.grid_cards.push_back(i);
            open_master_selection(g, 1, false); g.grid_for_upgrade = true;
        }
    }
    static SPIRE_FN void update(Game& g) {
        if (g.selected_cards.empty()) return;
        card_upgrade(g.run.master_deck[g.selected_cards[0]]); g.selected_cards.clear();
    }
};
}
