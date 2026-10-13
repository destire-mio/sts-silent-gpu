#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Hex : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
};
}
