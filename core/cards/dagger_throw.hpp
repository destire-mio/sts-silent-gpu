#pragma once
#include "../hooks.hpp"
#include "../actions/intent_flash.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct DaggerThrow : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 9, -1, -1, 3, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target) add_bottom(g, actions::VFX::make());
        queue_card_damage(g, card, target);
        add_bottom(g, actions::Draw::construct(1, true, -1, player_target));
        Effect e{};
        e.kind = EffectKind::Discard;
        e.source = e.target = player_target;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
