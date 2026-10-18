#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Repair : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onVictory(Game& g, Power& p, int) {
        if (g.player.creature.hp > 0)
            heal_creature(g, player_target, p.amount);
    }
};
}
