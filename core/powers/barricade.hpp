#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Barricade : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = -1; }
};
}
