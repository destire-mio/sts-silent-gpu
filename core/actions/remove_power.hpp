#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct RemovePower : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) { remove_power(g, e.target, e.power, e.power_instance); }
};
}
