#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct SacredBark : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        for (int slot = 0; slot < 2; ++slot)
            g.run.potion_state[slot] = construct_potion(g, g.run.potions[slot]);
    }
};
}
