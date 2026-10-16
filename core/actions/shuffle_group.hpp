#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ShuffleGroup : ActionHooks {
    SPIRE_SNAPSHOT("ShuffleAction.triggerRelics", scalar, effect.flag)
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("ShuffleAction.group", pile_group, effect.payload.state.ShuffleGroup.group)
    static SPIRE_FN constexpr ActionData data() { return {false, "SHUFFLE"}; }
    SPIRE_STATE(ShuffleGroup, PileId group;)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.flag)
            for (int i = next_relic_onShuffle(g, 0); i < g.run.relics.size; i = next_relic_onShuffle(g, i + 1)) relic_onShuffle(g, i);
        Pile& group = get_pile(g, e.payload.state.ShuffleGroup.group);
        shuffle(group.values, group.size, g.rng.shuffle);
    }
};
}
