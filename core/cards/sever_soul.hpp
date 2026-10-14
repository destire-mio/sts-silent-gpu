#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct SeverSoul : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 16, -1, -1, 6, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        Effect e{};
        e.kind = EffectKind::ExhaustAllNonAttack;
        e.source = e.target = no_target;
        add_bottom(g, e);
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
