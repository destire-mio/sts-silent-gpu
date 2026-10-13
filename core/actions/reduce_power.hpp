#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ReducePower : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) { reduce_power(g, e.target, e.power, e.amount); }
};
}
