#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct WarpedTongs : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
    static SPIRE_FN void atTurnStartPostDraw(Game& g, int relic) {
        Effect show{};
        show.kind = EffectKind::RelicAboveCreature;
        show.extra = relic;
        add_bottom(g, show);
        Effect upgrade{};
        upgrade.kind = EffectKind::UpgradeRandomCard;
        add_bottom(g, upgrade);
    }
};
}
