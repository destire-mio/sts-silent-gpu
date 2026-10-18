#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Waffle : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        increase_max_hp(g, 7);
        heal_creature(g, player_target, g.player.creature.max_hp);
    }
};
}
