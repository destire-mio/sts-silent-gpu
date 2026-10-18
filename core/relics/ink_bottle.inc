#pragma once
#include "core/hooks.hpp"
#include "core/actions/draw.hpp"

namespace spire::units::relics {
struct InkBottle : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {0, 250}; }
    static SPIRE_FN void onUseCard(Game& g, int relic, int, Effect&) {
        int& counter = g.run.relics[relic].counter;
        if (++counter == 10) {
            counter = 0;
            queue_relic_above(g, relic, player_target, false);
            add_bottom(g, units::actions::Draw::construct(1));
        }
    }
};
}
