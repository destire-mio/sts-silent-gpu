#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Shuffle : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect&) {
        shuffle(g.discard.values, g.discard.size, g.rng.shuffle);
        for (int i = 0; i < g.discard.size; ++i) g.draw.push_back(g.discard[i]);
        g.discard.clear();
    }
};
}
