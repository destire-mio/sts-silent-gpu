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
        for (int i = next_relic_onShuffle(g, 0); i < g.run.relics.size; i = next_relic_onShuffle(g, i + 1)) relic_onShuffle(g, i);
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        e.payload.state.Shuffle.shuffled = true;
        shuffle(g.discard.values, g.discard.size, g.rng.shuffle);
        for (int i = 0; i < g.discard.size; ++i) {
            ++e.payload.state.Shuffle.count;
            mark_card_move(g, g.discard[i]);
            g.draw.push_back(g.discard[i]);
        }
        g.discard.clear();
    }
};
}
