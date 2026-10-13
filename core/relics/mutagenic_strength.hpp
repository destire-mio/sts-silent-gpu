#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct MutagenicStrength : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        Effect strength{};
        strength.kind = EffectKind::ApplyPower;
        strength.source = strength.target = player_target;
        strength.power = PowerId::Strength;
        strength.amount = 3;
        add_top(g, strength);
        Effect lose = strength;
        lose.power = PowerId::LoseStrength;
        add_top(g, lose);
        add_top(g, relic_above_creature(player_target, relic));
    }
};
}
