#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct Cleric : EventHooks {
    SPIRE_STATE(Cleric, int healAmt; int purifyCost;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.Cleric", true}; }
    static SPIRE_FN bool canSpawn(const Game& g, EventId) { return g.run.gold >= 35; }
    static SPIRE_FN void enter(Game& g) {
        auto& s = g.event.state.Cleric;
        s.healAmt = g.run.gold >= 35 ? static_cast<int>(g.player.creature.max_hp * 0.25f) : 0;
        s.purifyCost = 75;
        event_options(g, 3);
        g.image_dialog[0].disabled = g.run.gold < 35; g.image_dialog[1].disabled = g.run.gold < s.purifyCost;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        if (g.event.screen != 0) { open_map(g); return; }
        if (option == 0) { lose_gold(g, 35); heal_creature(g, player_target, g.event.state.Cleric.healAmt); }
        if (option == 1) {
            event_purge_cards(g);
            if (!g.grid_cards.empty()) { lose_gold(g, g.event.state.Cleric.purifyCost); open_master_selection(g, 1, true); }
        }
        g.event.screen = 99;
        event_options(g, 1);
    }
    static SPIRE_FN void update(Game& g) {
        if (g.selected_cards.empty()) return;
        remove_master_card(g, g.selected_cards[0]);
        g.selected_cards.clear();
    }
};
}
