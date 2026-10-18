#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct FiendFire : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 2, 7, -1, -1, 3, 0, 0, true, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        Effect e{};
        e.kind = EffectKind::FiendFire;
        e.target = target;
        e.amount = e.info_base = g.cards[card].damage;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 3;
        }
    }
};
}
