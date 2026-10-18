#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct GainGold : ActionHooks {
    static SPIRE_FN void execute(Game& g, Effect& e) { gain_gold(g, e.amount); }
};
}
