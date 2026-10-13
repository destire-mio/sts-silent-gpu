#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct HandCheck : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("HandCheckAction.player", player)
    static SPIRE_FN void execute(Game& g, const Effect&) {
        for (int i = 0; i < g.hand.size; ++i) card_applyPowers(g, g.hand[i]);
    }
};
}
