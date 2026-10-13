#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Poison : PowerHooks {
    SPIRE_STATE(Poison, int source;)
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN void constructPower(Game&, Power& p, int, int source, int, int) {
        power_initialize_state(p);
        p.state.Poison.source = source;
        p.amount = min(9999, p.amount);
    }
};
}
