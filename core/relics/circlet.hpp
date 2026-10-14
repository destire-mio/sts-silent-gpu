#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Circlet : RelicHooks {
    static SPIRE_FN void onEquip(Game&, int) {}
    static SPIRE_FN void onUnequip(Game&, int) {}
    static SPIRE_FN constexpr RelicData data() { RelicData d{1, 400}; d.stacks_on_obtain = true; return d; }
};
}
