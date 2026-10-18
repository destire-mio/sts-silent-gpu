#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Astrolabe : RelicHooks {
    SPIRE_STATE(Astrolabe, bool cardsSelected;)
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void initializeState(Relic& relic) { relic.state.Astrolabe.cardsSelected = true; }
    static SPIRE_FN void giveCards(Game& g, int relic) {
        g.run.relics[relic].state.Astrolabe.cardsSelected = true;
        Card replacements[3]{};
        const int count = g.selected_cards.size;
        for (int i = 0; i < count; ++i) {
            const int index = g.selected_cards[i];
            Card original = g.run.master_deck[index];
            remove_master_card(g, index);
            for (int j = i + 1; j < count; ++j) if (g.selected_cards[j] > index) --g.selected_cards[j];
            replacements[i] = transform_card(g, original, true, g.rng.misc);
        }
        for (int i = 0; i < count; ++i) obtain_card(g, replacements[i]);
        g.selected_cards.clear();
    }
    static SPIRE_FN void onEquip(Game& g, int relic) {
        prepare_relic_selection(g);
        g.run.relics[relic].state.Astrolabe.cardsSelected = false;
        master_selection_cards(g, -1);
        if (g.grid_cards.empty()) {
            g.run.relics[relic].state.Astrolabe.cardsSelected = true;
        } else if (g.grid_cards.size <= 3) {
            g.selected_cards = g.grid_cards;
            giveCards(g, relic);
        } else {
            open_master_selection(g, 3, false);
        }
    }
    static SPIRE_FN void update(Game& g, int relic) {
        if (!g.run.relics[relic].state.Astrolabe.cardsSelected && g.selected_cards.size == 3) giveCards(g, relic);
    }
};
}
