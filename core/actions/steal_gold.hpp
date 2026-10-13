#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct StealGold : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        g.monsters[e.target].state.Looter.stolenGold += min(e.amount, g.run.gold);
    }
};
}
