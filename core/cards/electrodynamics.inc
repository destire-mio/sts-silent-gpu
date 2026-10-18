#pragma once
#include "core/hooks.hpp"
#include "core/actions/channel.hpp"
#include "core/actions/animate_orb.hpp"
#include "core/actions/evoke_orb.hpp"
#include "core/actions/evoke_without_removing_orb.hpp"
#include "core/actions/increase_max_orb.hpp"
#include "core/actions/decrease_max_orb.hpp"

namespace spire::units::cards {
struct Electrodynamics : CardHooks {
    static SPIRE_FN constexpr CardDef data() { return {CardType::Power, CardTarget::Self, 2, -1, -1, 2, 0, 0, 1, false, false}; }
    static SPIRE_FN void use(Game& g, int card, int ) {
        // Electrodynamics.use checks its card ID, but ElectroPower.ID is "Electro".
        queue_power(g, player_target, player_target, PowerId::Electro, -1);
        for (int i = 0; i < g.cards[card].magic; ++i) add_bottom(g, units::actions::Channel::make(make_orb(g, OrbKind::Lightning)));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1; card.magic = card.base_magic;
        }
    }
};
}
