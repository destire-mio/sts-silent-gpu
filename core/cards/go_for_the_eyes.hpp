#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct GoForTheEyes : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 0, 3, -1, 1, 1, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::ForTheEyes;
        e.source = e.target = no_target;
        e.extra = target;
        e.amount = g.cards[card].magic;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 1;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
