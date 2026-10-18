#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct WindmillStrike : CardHooks {
    static SPIRE_FN void onRetained(Game& g, int card) {
        g.cards[card].base_damage += g.cards[card].magic;
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
            card.base_damage += data().upgrade_damage;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
