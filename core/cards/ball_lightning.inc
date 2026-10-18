#pragma once
#include "core/hooks.hpp"
#include "core/actions/channel.hpp"
namespace spire::units::cards {
struct BallLightning : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 7, -1, 1, 3, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        for (int i = 0; i < g.cards[card].magic; ++i)
            add_bottom(g, actions::Channel::make(make_orb(g, OrbKind::Lightning)));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
