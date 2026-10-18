#pragma once
#include "core/hooks.hpp"
#include "core/card_pool.hpp"

namespace spire::units::relics {
struct PandorasBox : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    SPIRE_STATE(PandorasBox, uint8_t count; bool calledTransform;)
    static SPIRE_FN void initializeState(Relic& r) { r.state.PandorasBox.calledTransform = true; }
    static SPIRE_FN void onEquip(Game& g, int relic) {
        auto& s = g.run.relics[relic].state.PandorasBox;
        s.calledTransform = false;
        for (int i = 0; i < g.run.master_deck.size;) {
            if (g.run.master_deck[i].id != CardId::Strike_G && g.run.master_deck[i].id != CardId::Defend_G) { ++i; continue; }
            // PandorasBox.onEquip uses Iterator.remove without master-deck callbacks.
            detach_master_card(g, i);
            ++s.count;
        }
        if (s.count == 0) return;
        g.grid_cards.clear();
        for (int i = 0; i < s.count; ++i) {
            Card c = make_card(random_card(g));
            preview_card_obtain(g, c);
            g.grid_cards.insert(0, create_card(g, c));
        }
        g.selection_confirmation = true;
        g.grid_can_cancel = false;
        g.screen = Screen::Grid;
    }
    static SPIRE_FN CardId random_card(Game& g) {
        const CardPoolEntry* pools[] = {common_card_pool, uncommon_card_pool, rare_card_pool};
        int index = g.rng.card_random.random(g.run.source_card_pools[0].size + g.run.source_card_pools[1].size + g.run.source_card_pools[2].size - 1);
        for (int p = 0; p < 3; ++p) {
            if (index < g.run.source_card_pools[p].size) return pools[p][g.run.source_card_pools[p][index]].native;
            index -= g.run.source_card_pools[p].size;
        }
        SPIRE_ASSERT(false);
        __builtin_unreachable();
    }
    static SPIRE_FN void update(Game& g, int relic) {
        auto& s = g.run.relics[relic].state.PandorasBox;
        if (!s.calledTransform && g.screen != Screen::Grid) s.calledTransform = true;
    }
};
}
