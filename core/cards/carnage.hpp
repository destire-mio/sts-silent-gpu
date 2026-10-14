#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Carnage : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 20, -1, -1, 8, 0, 0, false, true};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        for (int i = 0; i < 6; ++i) {
            Effect e{};
            e.kind = EffectKind::VFX;
            e.source = e.target = no_target;
            add_bottom(g, e);
        }
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
