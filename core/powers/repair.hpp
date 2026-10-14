#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Repair : PowerHooks {
    SPIRE_WAITING_RULE("RepairPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onVictory(Game& g, Power& p, int) {
        if (g.player.creature.hp > 0)
            heal_creature(g, player_target, p.amount);
    }
};
}
