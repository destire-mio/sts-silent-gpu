#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Strawberry : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        g.player.creature.max_hp += 7;
        heal_creature(g, player_target, 7);
    }
};
}
