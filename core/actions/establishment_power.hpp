#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct EstablishmentPower : ActionHooks {
    SPIRE_SNAPSHOT("EstablishmentPowerAction.discountAmount", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        for (int i = 0; i < g.hand.size; ++i) {
            Card& c = g.cards[g.hand[i]];
            if (c.retain || c.self_retain)
                modify_cost_for_combat(c, -e.extra);
        }
    }
};
}
