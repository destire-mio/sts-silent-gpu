#pragma once
#include "../hooks.hpp"
#include "../actions/channel.hpp"
#include "../actions/animate_orb.hpp"
#include "../actions/evoke_orb.hpp"
#include "../actions/evoke_without_removing_orb.hpp"
#include "../actions/increase_max_orb.hpp"
#include "../actions/decrease_max_orb.hpp"

namespace spire::units::cards {
struct Capacitor : CardHooks {
    static SPIRE_FN constexpr CardDef data() { return {CardType::Power, CardTarget::Self, 1, -1, -1, 2, 0, 0, 1, false, false}; }
    static SPIRE_FN void use(Game& g, int card, int ) {
        add_bottom(g, units::actions::IncreaseMaxOrb::make(g.cards[card].magic));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1; card.magic = card.base_magic;
        }
    }
};
}
