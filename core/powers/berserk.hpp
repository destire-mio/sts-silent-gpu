#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::powers {
struct Berserk : PowerHooks {
    SPIRE_WAITING_RULE("BerserkPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int) {
        add_bottom(g, units::actions::GainEnergy::make(p.amount));
    }
};
}
