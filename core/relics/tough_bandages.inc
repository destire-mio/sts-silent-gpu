#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct ToughBandages : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void onManualDiscard(Game& g, int relic) {
        Effect show{};
        show.kind = EffectKind::RelicAboveCreature;
        show.extra = relic;
        add_bottom(g, show);
        queue_block(g, player_target, 3, true);
    }
};
}
