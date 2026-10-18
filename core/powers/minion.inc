#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Minion : PowerHooks {
    SPIRE_STATE(Minion, int8_t owner;)
    SPIRE_SNAPSHOT("AbstractPower.owner", creature, power.state.Minion.owner)
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int owner) {
        p.amount = -1;
        p.state.Minion.owner = owner;
    }
};
}
