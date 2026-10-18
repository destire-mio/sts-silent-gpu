#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Metallicize : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atEndOfTurnPreEndTurnCards(Game& g, Power& p, int owner, bool) {
        queue_block(g, owner, p.amount);
    }
};
}
