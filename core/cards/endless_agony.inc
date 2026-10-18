#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct EndlessAgony : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 0, 4, -1, -1, 2, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void triggerWhenDrawn(Game& g, int card) {
        Effect e{};
        e.kind = EffectKind::MakeTempCardInHand;
        e.card = create_card(g, card_makeStatEquivalentCopy(g, card));
        e.amount = 1;
        add_top(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
