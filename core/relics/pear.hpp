#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Pear : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        g.player.creature.max_hp += 10;
        heal_creature(g, player_target, 10);
    }
};
}
