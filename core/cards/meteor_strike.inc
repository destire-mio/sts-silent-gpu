#pragma once
#include "core/hooks.hpp"
#include "core/actions/channel.hpp"

namespace spire::units::cards {
struct MeteorStrike : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 5, 24, -1, 3, 6, 0, 0, false, false, false, false, 0, true, true};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        for (int i = 0; i < g.cards[card].magic; ++i)
            add_bottom(g, units::actions::Channel::make(make_orb(g, OrbKind::Plasma)));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 6;
        }
    }
};
}
