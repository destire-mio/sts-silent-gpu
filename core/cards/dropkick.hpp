#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Dropkick : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 5, -1, -1, 3, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        Effect e{};
        e.kind = EffectKind::Dropkick;
        e.source = player_target;
        e.target = target;
        e.info_base = g.cards[card].damage;
        e.amount = e.info_base;
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
