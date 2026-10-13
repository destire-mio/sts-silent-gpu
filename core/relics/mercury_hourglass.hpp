#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::relics {
struct MercuryHourglass : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) {
        add_bottom(g, relic_above_creature(player_target, relic));
        int amounts[monster_capacity]{};
        for (int i = 0; i < g.monster_order.size; ++i) amounts[i] = 3;
        add_bottom(g, units::actions::DamageAllEnemies::make(no_target, amounts, g.monster_order.size, DamageType::Thorns));
    }
};
}
