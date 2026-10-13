#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Damage : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true}; }
    SPIRE_FN static void execute(Game& g, const Effect& e) { damage(g, e); }
};
}
