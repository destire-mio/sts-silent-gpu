#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Beggar : EventHooks {
    SPIRE_STATE(Beggar, uint8_t screen;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.Beggar", true}; }
    SPIRE_SNAPSHOT("Beggar.screen", scalar, event.state.Beggar.screen == 0 ? "INTRO" : event.state.Beggar.screen == 1 ? "LEAVE" : "GAVE_MONEY")
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.gold >= 75; }
    static SPIRE_FN void enter(Game& g) { event_options(g, 2); g.image_dialog[0] = g.run.gold < 75; }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Beggar;
        if (s.screen == 0) {
            if (option == 0) { lose_gold(g, 75); s.screen = 2; }
            else s.screen = 1;
            event_options(g, 1);
        } else if (s.screen == 2) {
            event_purge_cards(g); open_master_selection(g, 1, true);
            s.screen = 1; event_options(g, 1);
        } else open_map(g);
    }
    static SPIRE_FN void update(Game& g) {
        if (g.selected_cards.empty()) return;
        remove_master_card(g, g.selected_cards[0]); g.selected_cards.clear(); open_map(g);
    }
};
}
