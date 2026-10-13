#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct EmptyCage : RelicHooks {
    SPIRE_STATE(EmptyCage, bool cardsSelected;)
    static SPIRE_FN void initializeState(Relic& relic) { relic.state.EmptyCage.cardsSelected = true; }
    static SPIRE_FN void deleteCards(Game& g, int relic) {
        g.run.relics[relic].state.EmptyCage.cardsSelected = true;
        for (int i = 0; i < g.selected_cards.size; ++i) {
            const int index = g.selected_cards[i];
            remove_master_card(g, index);
            for (int j = i + 1; j < g.selected_cards.size; ++j)
                if (g.selected_cards[j] > index) --g.selected_cards[j];
        }
        g.selected_cards.clear();
        g.room_phase = RoomPhase::Complete;
    }
    static SPIRE_FN void onEquip(Game& g, int relic) {
        g.run.relics[relic].state.EmptyCage.cardsSelected = false;
        g.room_phase = RoomPhase::Incomplete;
        g.grid_cards.clear();
        for (int i = 0; i < g.run.master_deck.size; ++i) {
            const CardId id = g.run.master_deck[i].id;
            if (card_def(id).purgeable)
                g.grid_cards.push_back(i);
        }
        if (g.grid_cards.empty()) {
            g.run.relics[relic].state.EmptyCage.cardsSelected = true;
        } else if (g.grid_cards.size <= 2) {
            for (int i = 0; i < g.grid_cards.size; ++i) g.selected_cards.push_back(g.grid_cards[i]);
            deleteCards(g, relic);
        } else open_master_selection(g, 2, true);
    }
    static SPIRE_FN void update(Game& g, int relic) {
        if (!g.run.relics[relic].state.EmptyCage.cardsSelected && g.selected_cards.size == 2)
            deleteCards(g, relic);
    }
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
};
}
