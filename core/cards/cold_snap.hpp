#pragma once
#include "../hooks.hpp"
#include "../actions/channel.hpp"
#include "../actions/animate_orb.hpp"
#include "../actions/evoke_orb.hpp"
#include "../actions/evoke_without_removing_orb.hpp"
#include "../actions/increase_max_orb.hpp"
#include "../actions/decrease_max_orb.hpp"

namespace spire::units::cards {
struct ColdSnap : CardHooks {
    static SPIRE_FN constexpr CardDef data() { return {CardType::Attack, CardTarget::Enemy, 1, 6, -1, 1, 3, 0, 0, false, false}; }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        for (int i = 0; i < g.cards[card].magic; ++i) add_bottom(g, units::actions::Channel::make(make_orb(g, OrbKind::Frost)));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 3;
        }
    }
};
}
