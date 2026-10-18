#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Split : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, false, false}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = -1; }
};
}
