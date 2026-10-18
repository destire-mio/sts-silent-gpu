#pragma once
#include "core/hooks.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::cards {
struct MindBlast : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 0, -1, -1, 0, 0, 0, false, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        add_bottom(g, actions::VFX::make());
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 1);
        }
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        get_card(g, card).base_damage = g.draw.size;
        apply_card_powers(g, card);
    }
    static SPIRE_FN void calculateCardDamage(Game& g, int card, int target) {
        calculate_card_damage(g, card, target);
    }
};
}
