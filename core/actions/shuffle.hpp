#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Shuffle : ActionHooks {
    SPIRE_FN static void prepare(Game& g, Effect& e) {
        e.source = e.target = no_target;
        for (int i = 0; i < g.run.relics.size; ++i) relic_onShuffle(g, i);
    }
    SPIRE_FN static void execute(Game& g, const Effect&) {
        shuffle(g.discard.values, g.discard.size, g.rng.shuffle);
        for (int i = 0; i < g.discard.size; ++i) g.draw.push_back(g.discard[i]);
        g.discard.clear();
    }
};
}
