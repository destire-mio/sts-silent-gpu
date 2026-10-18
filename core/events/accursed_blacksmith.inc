#pragma once
#include "core/hooks.hpp"

namespace spire::units::events {
struct AccursedBlacksmith : EventHooks {
    SPIRE_STATE(AccursedBlacksmith, int screenNum; bool pickCard;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.AccursedBlacksmith", true}; }
    static SPIRE_FN void enter(Game& g) {
        bool upgradeable = false;
        for (int i = 0; i < g.run.master_deck.size; ++i) upgradeable |= card_canUpgrade(g, -i - 2);
        event_options(g, 3); g.image_dialog[0].disabled = !upgradeable;
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.AccursedBlacksmith;
        if (s.screenNum) { open_map(g); return; }
        if (option == 0) {
            s.pickCard = true;
            g.grid_cards.clear();
            for (int i = 0; i < g.run.master_deck.size; ++i) if (card_canUpgrade(g, -i - 2)) g.grid_cards.push_back(i);
            open_master_selection(g, 1, false); g.grid_for_upgrade = true;
        } else if (option == 1) {
            queue_card_obtain(g, make_card(CardId::Pain));
            equip_relic(g, RelicId::WarpedTongs);
        }
        s.screenNum = 2; event_options(g, 1);
    }
    static SPIRE_FN void update(Game& g) {
        auto& s = g.event.state.AccursedBlacksmith;
        if (!s.pickCard || g.selected_cards.empty()) return;
        card_upgrade(g.run.master_deck[g.selected_cards[0]]);
        g.selected_cards.clear(); s.pickCard = false;
    }
};
}
