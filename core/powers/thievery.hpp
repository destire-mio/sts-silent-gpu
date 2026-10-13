#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Thievery : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, false, false}; }
};
}
