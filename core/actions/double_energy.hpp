#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DoubleEnergy : ActionHooks {
    static SPIRE_FN void execute(Game& g, Effect &) { gain_energy(g, g.player.energy); }
};
}
