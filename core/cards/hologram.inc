#pragma once
#include "core/hooks.hpp"
#include "core/actions/better_discard_pile_to_hand.hpp"

namespace spire::units::cards {
struct Hologram : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 3, -1, 0, 2, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        add_bottom(g, units::actions::BetterDiscardPileToHand::make(1));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.exhaust = false;
        }
    }
};
}
