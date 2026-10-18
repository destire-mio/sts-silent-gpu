#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct PotionBelt : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        g.run.potion_slots += 2;
        SPIRE_ASSERT(g.run.potion_slots <= potion_capacity);
        g.run.potions[g.run.potion_slots - 2] = PotionId::Empty;
        g.run.potions[g.run.potion_slots - 1] = PotionId::Empty;
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
