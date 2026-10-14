#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DoubleEnergy : ActionHooks {
    SPIRE_WAITING_RULE("DoubleEnergyAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN void execute(Game& g, Effect &) { gain_energy(g, g.player.energy); }
};
}
