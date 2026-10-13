#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Shuffle : ActionHooks {
    SPIRE_SNAPSHOT("EmptyDeckShuffleAction.triggerRelics", scalar, effect.flag)
    static SPIRE_FN constexpr ActionData data() { return {false, "SHUFFLE"}; }
    SPIRE_STATE(Shuffle, bool shuffled; int count;)
    SPIRE_FN static void prepare(Game& g, Effect& e) {
        action_initialize_state(e);
        e.source = e.target = no_target;
        for (int i = 0; i < g.run.relics.size; ++i) relic_onShuffle(g, i);
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        e.state.Shuffle.shuffled = true;
        shuffle(g.discard.values, g.discard.size, g.rng.shuffle);
        for (int i = 0; i < g.discard.size; ++i) {
            ++e.state.Shuffle.count;
            g.cards[g.discard[i]].pending_clear = true;
            g.draw.push_back(g.discard[i]);
        }
        g.discard.clear();
    }
};
}
