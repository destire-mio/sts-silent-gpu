#pragma once
#include "../hooks.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::cards {
struct ThroughViolence : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 0, 20, -1, -1, 10, 0, 0, true, false};
        d.self_retain = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target)
            add_bottom(g, actions::VFX::make());
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 10;
        }
    }
};
}
