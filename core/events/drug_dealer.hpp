#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct DrugDealer : EventHooks {
    SPIRE_STATE(DrugDealer, int screenNum; bool cardsSelected;)
    static SPIRE_FN constexpr EventData data() { return {"events.city.DrugDealer", true}; }
    static SPIRE_FN void enter(Game& g) {
        master_selection_cards(g, -1);
        event_options(g, 3); g.image_dialog[1].disabled = g.grid_cards.size < 2; g.grid_cards.clear();
    }
    static SPIRE_FN void choose(Game& g, int option) {
        auto& s = g.event.state.DrugDealer;
        if (s.screenNum) { open_map(g); return; }
        if (option == 0) queue_card_obtain(g, make_card(CardId::JAX));
        else if (option == 1) { master_selection_cards(g, -1); open_master_selection(g, 2, false); }
        else equip_relic(g, has_relic(g, "MutagenicStrength") ? RelicId::Circlet : RelicId::MutagenicStrength);
        s.screenNum = 1; event_options(g, 1);
    }
    static SPIRE_FN void update(Game& g) {
        auto& s = g.event.state.DrugDealer;
        if (s.cardsSelected || g.selected_cards.size != 2) return;
        s.cardsSelected = true;
        const Card cards[] = {g.run.master_deck[g.selected_cards[0]], g.run.master_deck[g.selected_cards[1]]};
        for (int i = 0; i < 2; ++i) {
            int index = 0; while (g.run.master_deck[index].uuid != cards[i].uuid) ++index;
            remove_master_card(g, index);
            queue_card_obtain(g, transform_card(g, cards[i], false, g.rng.misc));
        }
        g.selected_cards.clear();
    }
};
}
