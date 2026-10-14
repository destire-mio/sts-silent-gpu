#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Bias : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int owner) {
        queue_power(g, owner, owner, PowerId::Focus, -p.amount);
    }
    static SPIRE_FN void stackPower(Game &, Power& p, int, int amount) { p.amount += amount; }
};
}
