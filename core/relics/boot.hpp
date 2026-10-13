#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Boot : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN int onAttackToChangeDamage(Game& g, int relic, const DamageInfo& info, int amount) {
        if (info.source != no_target && info.damage_type == DamageType::Normal && amount > 0 && amount < 5) {
            queue_relic_display(g, relic, player_target, false);
            return 5;
        }
        return amount;
    }
};
}
