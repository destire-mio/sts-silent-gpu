#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct WheelKick : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 15, -1, 2, 5, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        add_bottom(g, actions::Draw::construct(g.cards[card].magic, true, -1, player_target));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_card_damage(card, data().upgrade_damage);
        }
    }
};
}
