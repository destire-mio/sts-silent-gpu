#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct FossilizedHelix : RelicHooks {
    SPIRE_STATE(FossilizedHelix, bool grayscale;)
    SPIRE_SNAPSHOT("AbstractRelic.grayscale", scalar, relic.state.FossilizedHelix.grayscale)
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        queue_relic_above(g, relic, player_target, false);
        Effect e{};
        e.kind = EffectKind::ApplyPower;
        e.power = PowerId::Buffer;
        e.amount = 1;
        add_bottom(g, e);
        g.run.relics[relic].state.FossilizedHelix.grayscale = true;
    }
    static SPIRE_FN void justEnteredRoom(Game& g, int relic, int) { g.run.relics[relic].state.FossilizedHelix.grayscale = false; }
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
};
}
