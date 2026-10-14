#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct MasterReality : PowerHooks {
    SPIRE_WAITING_RULE("MasterRealityPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game &, Power& p, int) { p.amount = -1; }
};
}
