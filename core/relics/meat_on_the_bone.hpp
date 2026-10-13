#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct MeatOnTheBone : RelicHooks {
    using RelicHooks::onTrigger;
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 250}; d.triggers_at_battle_end = true; return d; }
    static SPIRE_FN void onTrigger(Game& g, int relic) {
        const Creature& p = g.player.creature;
        if (static_cast<float>(p.hp) <= static_cast<float>(p.max_hp) / 2.0f && p.hp > 0) {
            queue_relic_above(g, relic, player_target, true);
            heal_creature(g, player_target, 12);
        }
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 48; }
};
}
