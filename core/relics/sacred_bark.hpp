#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct SacredBark : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 999}; d.potion_multiplier = 2; return d; }
    static SPIRE_FN void onEquip(Game& g, int) {
        for (int slot = 0; slot < 2; ++slot)
            g.run.potion_state[slot] = construct_potion(g, g.run.potions[slot]);
    }
};
}
