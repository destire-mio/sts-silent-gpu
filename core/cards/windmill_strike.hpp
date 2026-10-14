#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct WindmillStrike : CardHooks {
    static SPIRE_FN void onRetained(Game& g, int card) {
        // WindmillStrike.onRetained sets upgradedDamage without upgrading the card's name.
        upgrade_card_damage(g.cards[card], g.cards[card].magic);
    }
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 2, 7, -1, 4, 3, 0, 1, false, false};
        d.self_retain = true;
        d.strike = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_card_damage(card, data().upgrade_damage);
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
