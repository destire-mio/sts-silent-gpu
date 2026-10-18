#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct SashWhip : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 8, -1, 1, 2, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::HeadStomp;
        e.source = no_target;
        e.target = target;
        e.extra = g.cards[card].magic;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 2;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
