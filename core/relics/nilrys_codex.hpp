#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct NilrysCodex : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
    static SPIRE_FN void onPlayerEndTurn(Game& g, int relic) {
        queue_relic_above(g, relic, player_target);
        Effect e{};
        e.kind = EffectKind::Codex;
        add_bottom(g, e);
    }
};
}
