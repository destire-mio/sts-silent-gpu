#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::relics {
struct InkBottle : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {0, 250}; }
    static SPIRE_FN void onUseCard(Game& g, int relic, int, Effect&) {
        int& counter = g.run.relics[relic].counter;
        if (++counter == 10) {
            counter = 0;
            add_bottom(g, relic_above_creature(player_target, relic));
            add_bottom(g, units::actions::Draw::construct(1));
        }
    }
};
}
