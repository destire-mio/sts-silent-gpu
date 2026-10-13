#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct StealGold : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        g.monsters[e.target].stolen_gold += min(e.amount, g.run.gold);
    }
};
}
