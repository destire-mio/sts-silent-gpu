#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct ModeShift : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
};
}
