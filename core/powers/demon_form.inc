#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct DemonForm : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurnPostDraw(Game& g, Power& p, int owner) {
        queue_power(g, owner, owner, PowerId::Strength, p.amount);
    }
};
}
