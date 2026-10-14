#pragma once
#include "../hooks.hpp"

namespace spire::units::events {
struct Bonfire : EventHooks {
    SPIRE_STATE(Bonfire, uint8_t screen; uint8_t offeredCard; bool cardSelect;)
    static SPIRE_FN constexpr EventData data() { return {"events.shrines.Bonfire", true}; }
    SPIRE_SNAPSHOT("Bonfire.screen", scalar, event.state.Bonfire.screen == 0 ? "INTRO" : event.state.Bonfire.screen == 1 ? "CHOOSE" : "COMPLETE")
    SPIRE_SNAPSHOT("Bonfire.offeredCard", card, static_cast<int>(event.state.Bonfire.offeredCard) - 1)
    static SPIRE_FN void enter(Game& g) { event_options(g, 1); }
    static SPIRE_FN void choose(Game& g, int) {
        auto& s = g.event.state.Bonfire;
        if (s.screen == 0) s.screen = 1;
        else if (s.screen == 1) {
            event_purge_cards(g);
            if (!g.grid_cards.empty()) { open_master_selection(g, 1, true); s.cardSelect = true; }
            else s.screen = 2;
        } else open_map(g);
    }
    static SPIRE_FN void update(Game& g) {
        auto& s = g.event.state.Bonfire;
        if (!s.cardSelect || g.selected_cards.empty()) return;
        const int chosen = g.selected_cards[0];
        const Card card = g.run.master_deck[chosen];
        s.offeredCard = g.cards.add(card) + 1;
        switch (card_rarity(card.id)) {
        case CardRarity::Curse: equip_relic(g, has_relic(g, "Spirit Poop") ? RelicId::Circlet : RelicId::SpiritPoop); break;
        case CardRarity::Common: case CardRarity::Special: heal_creature(g, player_target, 5); break;
        case CardRarity::Uncommon: heal_creature(g, player_target, g.player.creature.max_hp); break;
        case CardRarity::Rare: increase_max_hp(g, 10); heal_creature(g, player_target, g.player.creature.max_hp); break;
        case CardRarity::Basic: break;
        }
        remove_master_card(g, chosen);
        g.selected_cards.clear(); s.screen = 2; s.cardSelect = false;
    }
};
}
