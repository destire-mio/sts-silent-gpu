#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Torii : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN int onAttacked(Game& g, int relic, const DamageInfo& info, int amount) {
        if (info.source != no_target && info.damage_type != DamageType::HpLoss &&
            info.damage_type != DamageType::Thorns && amount > 1 && amount <= 5) {
        Effect show{};
        show.kind = EffectKind::RelicAboveCreature;
        show.extra = relic;
        add_bottom(g, show);
            return 1;
        }
        return amount;
    }
};
}
