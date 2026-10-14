#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct EnergyDown : PowerHooks {
    SPIRE_WAITING_RULE("EnergyDownPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int) {
        Effect e{};
        e.kind = EffectKind::LoseEnergy;
        e.amount = p.amount;
        e.source = e.target = no_target;
        add_bottom(g, e);
    }
};
}
