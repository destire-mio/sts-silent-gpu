#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct SpiritShield : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 2, -1, -1, 3, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        applyPowers(g, card);
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        int count = 0;
        for (int i = 0; i < g.hand.size; ++i)
            if (g.hand[i] != card) ++count;
        g.cards[card].base_block_adjust = count * g.cards[card].magic + 1;
        apply_card_powers(g, card);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
