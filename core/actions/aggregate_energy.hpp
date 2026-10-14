#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct AggregateEnergy : ActionHooks {
    SPIRE_SNAPSHOT("AggregateEnergyAction.divideAmount", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) { gain_energy(g, g.draw.size / e.extra); }
};
}
