#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Headbutt : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 9, -1, -1, 3, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::DiscardPileToTopOfDeck;
        action_initialize_state(e);
        e.target = no_target;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
