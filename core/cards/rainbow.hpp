#pragma once
#include "../hooks.hpp"
#include "../actions/channel.hpp"

namespace spire::units::cards {
struct Rainbow : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 2, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        add_bottom(g, units::actions::Channel::make(make_orb(g, OrbKind::Lightning)));
        add_bottom(g, units::actions::Channel::make(make_orb(g, OrbKind::Frost)));
        add_bottom(g, units::actions::Channel::make(make_orb(g, OrbKind::Dark)));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.exhaust = false;
        }
    }
};
}
