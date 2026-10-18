#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Matryoshka : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { RelicData d{2, 250}; d.boss_chest_excluded = true; return d; }
    SPIRE_STATE(Matryoshka, bool usedUp;)
    SPIRE_SNAPSHOT("AbstractRelic.usedUp", scalar, relic.state.Matryoshka.usedUp)
    SPIRE_SNAPSHOT("AbstractRelic.grayscale", scalar, relic.state.Matryoshka.usedUp)
    static SPIRE_FN void setCounter(Game& g, int relic, int value) {
        g.run.relics[relic].counter = value;
        if (value == -2) g.run.relics[relic].state.Matryoshka.usedUp = true;
    }
    static SPIRE_FN void onChestOpen(Game& g, int relic, bool boss_chest) {
        if (!boss_chest && g.run.relics[relic].counter > 0) {
            --g.run.relics[relic].counter;
            if (g.room_in_combat) queue_relic_above(g, relic, player_target, true);
            const RelicTier tier = g.rng.relic.randomBoolean(0.75f) ? RelicTier::Common : RelicTier::Uncommon;
            add_relic_reward(g, tier);
            if (g.run.relics[relic].counter == 0) setCounter(g, relic, -2);
        }
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 40; }
};
}
