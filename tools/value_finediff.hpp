#pragma once
#include "core/engine.hpp"

namespace spire::finediff {
SPIRE_FN inline bool perturb(Game& game, int change) {
    if (change == 0 || change == 1) {
        const int delta = change == 0 ? 5 : 10;
        if (game.player.creature.hp <= delta) return false;
        game.player.creature.hp -= delta;
        return true;
    }
    if (change == 2) {
        for (int i = 0; i < game.run.potion_slots; ++i) {
            if (game.run.potions[i] != PotionId::Empty) {
                game.run.potions[i] = PotionId::Empty;
                return true;
            }
        }
        return false;
    }
    SPIRE_ASSERT(change == 3);
    if (game.run.master_deck.size == master_deck_capacity) return false;
    Card card = make_card(CardId::Injury);
    card.uuid = game.next_card_uuid++;
    game.run.master_deck.push_back(card);
    return true;
}
}
