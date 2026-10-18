#pragma once
#include "core/hooks.hpp"
#include "core/actions/doppelganger.hpp"

namespace spire::units::cards {
struct Doppelganger : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, -1, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        const Card& c = g.cards[card];
        add_bottom(g, actions::Doppelganger::construct(c.upgrades > 0, c.free_to_play_once, c.energy_on_use));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
