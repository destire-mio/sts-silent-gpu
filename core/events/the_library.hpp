#pragma once
#include "../hooks.hpp"
#include "../card_pool.hpp"

namespace spire::units::events {
struct TheLibrary : EventHooks {
    SPIRE_STATE(TheLibrary, int healAmt; uint8_t screenNum; bool pickCard;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.TheLibrary", true}; }
    static SPIRE_FN void enter(Game& g) {
        g.event.state.TheLibrary.healAmt = floor_int(g.player.creature.max_hp * 0.2f + 0.5f);
        event_options(g, 2);
    }
    static SPIRE_FN Card draw(Game& g) {
        const int roll = g.rng.card.random(99) + g.run.card_blizz_randomizer;
        g.room_rare_card_chance = 3; g.room_uncommon_card_chance = 37;
        for (int i = 0; i < g.run.relics.size; ++i) {
            g.room_rare_card_chance = relic_changeRareCardRewardChance(g, i, g.room_rare_card_chance);
            g.room_uncommon_card_chance = relic_changeUncommonCardRewardChance(g, i, g.room_uncommon_card_chance);
        }
        const CardPoolEntry* pools[] = {common_card_pool, uncommon_card_pool, rare_card_pool};
        const int sizes[] = {sizeof(common_card_pool) / sizeof(*common_card_pool), sizeof(uncommon_card_pool) / sizeof(*uncommon_card_pool), sizeof(rare_card_pool) / sizeof(*rare_card_pool)};
        const int rarity = roll < g.room_rare_card_chance ? 2 : roll < g.room_rare_card_chance + g.room_uncommon_card_chance ? 1 : 0;
        return make_card(card_id(pools[rarity][g.rng.card.random(sizes[rarity] - 1)].id));
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.TheLibrary;
        if (s.screenNum) { open_map(g); return; }
        s.screenNum = 1; event_options(g, 1);
        if (option != 0) { heal_creature(g, player_target, s.healAmt); return; }
        s.pickCard = true; g.grid_cards.clear();
        for (int i = 0; i < 20; ++i) {
            Card card = draw(g);
            bool duplicate;
            do {
                duplicate = false;
                for (int j = 0; j < g.grid_cards.size; ++j) if (g.cards[g.grid_cards[j]].id == card.id) { duplicate = true; break; }
                if (duplicate) card = draw(g);
            } while (duplicate);
            preview_card_obtain(g, card);
            g.grid_cards.insert(0, create_card(g, card));
        }
        open_grid_selection(g, PileId::Grid, 1, false);
        g.grid_can_cancel = true;
    }
    static SPIRE_FN void update(Game& g) {
        if (!g.event.state.TheLibrary.pickCard || g.selected_cards.empty()) return;
        queue_card_obtain(g, make_card(g.cards[g.selected_cards[0]].id));
        g.selected_cards.clear();
    }
};
}
