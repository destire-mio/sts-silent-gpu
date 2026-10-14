#pragma once
#include "../api.hpp"
#include "../actions/gain_energy.hpp"
#include "../actions/change_stance.hpp"
SPIRE_RULE("rule:stances.CalmStance")

namespace spire::units::stances {
struct Calm {
    static SPIRE_FN void onExit(Game& g) { add_bottom(g, units::actions::GainEnergy::make(2)); }
};
}
