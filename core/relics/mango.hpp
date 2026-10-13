#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Mango : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        g.player.creature.max_hp += 14;
        heal_creature(g, player_target, 14);
    }
};
}
