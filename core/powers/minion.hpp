#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Minion : PowerHooks {
    SPIRE_STATE(Minion, int actual_owner;)
    SPIRE_SNAPSHOT("AbstractPower.owner", creature, power.state.Minion.actual_owner)
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int owner) { p.amount = -1; p.state.Minion.actual_owner = owner; }
};
}
