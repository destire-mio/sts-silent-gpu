#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct FlurryOfBlows : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 0, 4, -1, -1, 2, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void triggerExhaustedCardsOnStanceChange(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::DiscardToHand;
        e.source = e.target = no_target;
        e.card = card;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 2;
        }
    }
};
}
