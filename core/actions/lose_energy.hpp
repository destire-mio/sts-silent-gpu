#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct LoseEnergy : ActionHooks {
    SPIRE_STATE(LoseEnergy, int energyLoss;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        const int amount = e.amount;
        action_initialize_state(e);
        e.source = e.target = player_target;
        e.amount = 0;
        e.payload.state.LoseEnergy.energyLoss = amount;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        g.player.energy = max(0, g.player.energy - e.payload.state.LoseEnergy.energyLoss);
    }
};
}
