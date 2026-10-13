#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Tingsha : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void onManualDiscard(Game& g, int relic) {
        Effect show{};
        show.kind = EffectKind::RelicAboveCreature;
        show.extra = relic;
        add_bottom(g, show);
        Effect hit{};
        hit.kind = EffectKind::DamageRandomEnemy;
        hit.amount = hit.info_base = 3;
        hit.damage_type = DamageType::Thorns;
        add_bottom(g, hit);
    }
};
}
