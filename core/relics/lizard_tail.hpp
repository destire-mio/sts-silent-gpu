#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct LizardTail : RelicHooks {
    using RelicHooks::onTrigger;
    static SPIRE_FN constexpr RelicData data() { RelicData d{-1, 300}; d.revives_player = true; return d; }
    SPIRE_STATE(LizardTail, bool usedUp;)
    SPIRE_SNAPSHOT("AbstractRelic.usedUp", scalar, relic.state.LizardTail.usedUp)
    SPIRE_SNAPSHOT("AbstractRelic.grayscale", scalar, relic.state.LizardTail.usedUp)
    static SPIRE_FN void setCounter(Game& g, int relic, int value) {
        if (value == -2) {
            g.run.relics[relic].state.LizardTail.usedUp = true;
            g.run.relics[relic].counter = -2;
        }
    }
    static SPIRE_FN void onTrigger(Game& g, int relic) {
        queue_relic_above(g, relic, player_target, true);
        heal_creature(g, player_target, max(1, g.player.creature.max_hp / 2));
        setCounter(g, relic, -2);
    }
};
}
