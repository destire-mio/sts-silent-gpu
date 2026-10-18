#pragma once
#include "core/hooks.hpp"
#include "core/actions/intent_flash.hpp"

namespace spire::units::cards {
struct Feed : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 10, -1, 3, 2, 0, 1, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target) add_bottom(g, actions::VFX::make());
        Effect e{};
        e.kind = EffectKind::Feed;
        e.target = target;
        e.amount = e.info_base = g.cards[card].damage;
        e.extra = g.cards[card].magic;
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
