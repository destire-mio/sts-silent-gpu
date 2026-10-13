#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AddCardToDeck : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("AddCardToDeckAction.cardToObtain", card, effect.card)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        const CardRarity rarity = card_rarity(g.cards[e.card].id);
        if (rarity != CardRarity::Basic && rarity != CardRarity::Special && rarity != CardRarity::Curse)
            ++g.obtained_cards[static_cast<int>(g.cards[e.card].id)];
        obtain_card(g, g.cards[e.card]);
    }
};
}
