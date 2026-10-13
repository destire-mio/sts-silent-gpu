#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct DollysMirror : RelicHooks {
    SPIRE_STATE(DollysMirror, bool cardSelected;)
    static SPIRE_FN void initialState(Game& g, int relic) { g.run.relics[relic].state.DollysMirror.cardSelected = true; }
    static SPIRE_FN void onEquip(Game& g, int relic) {
        g.run.relics[relic].state.DollysMirror.cardSelected = false;
        g.room_phase = RoomPhase::Incomplete;
        g.grid_cards.clear();
        for (int i = 0; i < g.run.master_deck.size; ++i) g.grid_cards.push_back(i);
        open_master_selection(g, 1, false);
    }
    static SPIRE_FN void update(Game& g, int relic) {
        if (!g.run.relics[relic].state.DollysMirror.cardSelected && g.selected_cards.size == 1) {
            g.run.relics[relic].state.DollysMirror.cardSelected = true;
            Card copy = card_makeStatEquivalentCopy(g, -g.selected_cards[0] - 2);
            queue_card_obtain(g, copy);
            g.selected_cards.clear();
            g.room_phase = RoomPhase::Complete;
        }
    }
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
};
}
