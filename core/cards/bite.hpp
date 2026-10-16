#pragma once
#include "../hooks.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::cards {
struct Bite : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 7, -1, 2, 1, 0, 1, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target) add_bottom(g, actions::VFX::make());
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::Heal;
        e.amount = g.cards[card].magic;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
