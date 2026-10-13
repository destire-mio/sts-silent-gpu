#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Akabeko : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        e.power = PowerId::Vigor;
        e.amount = 8;
        add_top(g, e);
        queue_relic_above(g, relic, player_target, true);
    }
};
}
