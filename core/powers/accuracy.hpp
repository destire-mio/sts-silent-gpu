#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Accuracy : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) { p.amount += amount; }
};
}
