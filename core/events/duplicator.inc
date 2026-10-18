#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct Duplicator : EventHooks {
    SPIRE_STATE(Duplicator, int screenNum;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.Duplicator", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.act == 2 || g.run.act == 3; }
    static SPIRE_FN void enter(Game& g) { event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.Duplicator;
        if (s.screenNum == 0) {
            s.screenNum = 2; event_options(g, 1);
            if (option == 0) {
                g.grid_cards.clear();
                for (int i = 0; i < g.run.master_deck.size; ++i) g.grid_cards.push_back(i);
                open_master_selection(g, 1, false); g.grid_is_master_deck = true;
            }
        } else open_map(g);
    }
    static SPIRE_FN void update(Game& g) {
        if (g.selected_cards.empty()) return;
        Card copy = card_makeStatEquivalentCopy(g, -g.selected_cards[0] - 2);
        copy.bottle_flame = copy.bottle_lightning = copy.bottle_tornado = false;
        queue_card_obtain(g, copy); g.selected_cards.clear();
    }
};
}
