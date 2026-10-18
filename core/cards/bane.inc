#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Bane : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 7, -1, -1, 3, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::Bane;
        e.source = player_target;
        e.target = target;
        e.info_base = e.amount = g.cards[card].damage;
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
