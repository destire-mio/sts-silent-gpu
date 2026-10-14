#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Stack : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 0, -1, 0, 3, 0, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        g.cards[card].base_block_adjust = g.discard.size;
        apply_card_powers(g, card);
    }
};
}
