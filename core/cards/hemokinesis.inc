#pragma once
#include "core/hooks.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::cards {
struct Hemokinesis : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 15, -1, 2, 5, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target) add_bottom(g, actions::VFX::make());
        Effect e{};
        e.kind = EffectKind::LoseHP;
        e.amount = g.cards[card].magic;
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
