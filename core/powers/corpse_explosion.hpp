#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct CorpseExplosion : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = 1; }
};
}
