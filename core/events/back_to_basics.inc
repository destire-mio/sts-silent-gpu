#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct BackToBasics : EventHooks {
    SPIRE_STATE(BackToBasics, uint8_t screen; uint8_t upgradedCount;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.BackToBasics", true}; }
    SPIRE_SNAPSHOT("BackToBasics.screen", scalar, event.state.BackToBasics.screen ? "COMPLETE" : "INTRO")
    SPIRE_SNAPSHOT("BackToBasics.cardsUpgraded", scalar, [&]() -> pybind11::object { pybind11::list result; for (int i = 0; i < event.state.BackToBasics.upgradedCount; ++i) result.append(card_names[static_cast<int>(graph.game.grid_cards[i])]); return result; }())
    static SPIRE_FN void enter(Game& g) { g.grid_cards.clear(); event_options(g, 2); }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.BackToBasics;
        if (s.screen) { open_map(g); return; }
        if (option == 0) {
            event_purge_cards(g);
            if (!g.grid_cards.empty()) { open_master_selection(g, 1, false); g.grid_can_cancel = true; }
        } else {
            for (int i = 0; i < g.run.master_deck.size; ++i) {
                Card& card = g.run.master_deck[i];
                if ((card.id == CardId::Strike_G || card.id == CardId::Defend_G) && card_canUpgrade(g, -i - 2)) {
                    card_upgrade(card); g.grid_cards.push_back(static_cast<int>(card.id)); ++s.upgradedCount;
                }
            }
        }
        s.screen = 1; event_options(g, 1);
    }
    static SPIRE_FN void update(Game& g) {
        if (g.selected_cards.empty()) return;
        remove_master_card(g, g.selected_cards[0]); g.selected_cards.clear(); g.grid_cards.clear();
    }
};
}
