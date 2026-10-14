#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct DemonForm : PowerHooks {
    SPIRE_WAITING_RULE("DemonFormPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurnPostDraw(Game& g, Power& p, int owner) {
        queue_power(g, owner, owner, PowerId::Strength, p.amount);
    }
};
}
