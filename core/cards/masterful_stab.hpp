#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct MasterfulStab : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 0, 12, -1, -1, 4, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void tookDamage(Game& g, int card) {
        Card& c = g.cards[card];
        const int diff = c.cost - c.cost_for_turn;
        ++c.cost;
        c.cost_for_turn = max(0, c.cost - diff);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
