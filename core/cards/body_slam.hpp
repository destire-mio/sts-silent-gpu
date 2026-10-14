#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct BodySlam : CardHooks {
    static SPIRE_FN void applyPowers(Game& g, int card) {
        get_card(g, card).base_damage = g.player.creature.block;
        apply_card_powers(g, card);
    }
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 0, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        g.cards[card].base_damage = g.player.creature.block;
        card_calculateCardDamage(g, card, target);
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 0);
        }
    }
};
}
