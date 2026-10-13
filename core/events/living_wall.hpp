#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct LivingWall : EventHooks {
    SPIRE_STATE(LivingWall, uint8_t screen; uint8_t choice; bool pickCard;)
    static SPIRE_FN constexpr EventData data() { return {"events.exordium.LivingWall", true}; }
    SPIRE_SNAPSHOT("LivingWall.screen", scalar, event.state.LivingWall.screen ? "RESULT" : "INTRO")
    SPIRE_SNAPSHOT("LivingWall.choice", scalar, event.state.LivingWall.choice == 0 ? nullptr : event.state.LivingWall.choice == 1 ? "FORGET" : event.state.LivingWall.choice == 2 ? "CHANGE" : "GROW")
    static SPIRE_FN void enter(Game& g) {
        event_options(g, 3);
        bool upgradable = false;
        for (int i = 0; i < g.run.master_deck.size; ++i) upgradable |= card_canUpgrade(g, -i - 2);
        g.image_dialog[2].disabled = !upgradable;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.LivingWall;
        if (s.screen) { open_map(g); return; }
        s.choice = option + 1;
        event_purge_cards(g);
        if (!g.grid_cards.empty()) {
            if (option == 2) {
                g.grid_cards.clear();
                for (int i = 0; i < g.run.master_deck.size; ++i)
                    if (card_canUpgrade(g, -i - 2)) g.grid_cards.push_back(i);
            }
            open_master_selection(g, 1, option == 0);
            g.grid_for_upgrade = option == 2; g.grid_for_transform = option == 1;
        }
        s.pickCard = true; s.screen = 1; event_options(g, 1);
    }
    static SPIRE_FN void update(Game& g) {
        auto& s = g.event.state.LivingWall;
        if (!s.pickCard || g.selected_cards.empty()) return;
        const int chosen = g.selected_cards[0];
        if (s.choice == 3) card_upgrade(g.run.master_deck[chosen]);
        else {
            const Card original = g.run.master_deck[chosen];
            remove_master_card(g, chosen);
            if (s.choice == 2) queue_card_obtain(g, transform_card(g, original, false, g.rng.misc));
        }
        g.selected_cards.clear(); s.pickCard = false;
    }
};
}
