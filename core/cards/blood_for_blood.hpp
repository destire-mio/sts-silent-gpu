#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct BloodForBlood : CardHooks {
    static SPIRE_FN void tookDamage(Game& g, int card) {
        update_card_cost(g.cards[card], -1);
    }
    static SPIRE_FN Card makeCopy(Game& g, int) {
        Card copy = make_card(CardId::BloodForBlood);
        update_card_cost(copy, -g.player.damaged_this_combat);
        return copy;
    }
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 4, 18, -1, -1, 4, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            // BloodForBlood.upgrade can pass -1 before clamping only the base cost.
            upgrade_card_base_cost(card, card.cost < 4 ? card.cost - 1 : 3);
            if (card.cost < 0) card.cost = 0;
            upgrade_card_damage(card, data().upgrade_damage);
        }
    }
};
}
