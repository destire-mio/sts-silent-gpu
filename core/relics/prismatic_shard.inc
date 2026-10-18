#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct PrismaticShard : RelicHooks {
    static SPIRE_FN constexpr RelicData data() {
        RelicData d{-1, 150};
        d.any_color_rewards = true;
        d.empty_orb_class = "orbs.EmptyOrbSlot";
        return d;
    }
    static SPIRE_FN void onEquip(Game& g, int) {
        if (g.player.master_max_orbs == 0) g.player.master_max_orbs = 1;
    }
};
}
