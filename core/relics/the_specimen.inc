#pragma once
#include "core/hooks.hpp"
#include "core/actions/apply_power_to_random_enemy.hpp"

namespace spire::units::relics {
struct TheSpecimen : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void onMonsterDeath(Game& g, int, int owner) {
        if (has_power(g, g.monsters[owner].creature, PowerId::Poison) && !monsters_basically_dead(g)) {
            const int amount = power_amount(g, g.monsters[owner].creature, PowerId::Poison);
            Power poison{PowerId::Poison, amount};
            power_constructPower(g, poison, no_target, player_target, 0, -1);
            add_bottom(g, actions::ApplyPowerToRandomEnemy::construct(player_target, poison, amount));
        }
    }
};
}
