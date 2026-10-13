#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ShuffleGroup : ActionHooks {
    SPIRE_STATE(ShuffleGroup, PileId group;)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.flag)
            for (int i = 0; i < g.run.relics.size; ++i) relic_onShuffle(g, i);
        Pile& group = get_pile(g, e.state.ShuffleGroup.group);
        shuffle(group.values, group.size, g.rng.shuffle);
    }
};
}
