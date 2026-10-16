#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ShuffleAll : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "SHUFFLE"}; }
    SPIRE_STATE(ShuffleAll, bool shuffled; bool vfxDone; int count;)
    static SPIRE_FN void prepare(Game& g, Effect& e) {
        action_initialize_state(e);
        e.source = e.target = no_target;
        for (int i = next_relic_onShuffle(g, 0); i < g.run.relics.size; i = next_relic_onShuffle(g, i + 1))
            relic_onShuffle(g, i);
    }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (!e.payload.state.ShuffleAll.shuffled) {
            Effect a{};
            a.kind = EffectKind::PutOnDeck;
            a.amount = 99;
            a.flag = true;
            add_top(g, a);
            shuffle(g.discard.values, g.discard.size, g.rng.shuffle);
            e.payload.state.ShuffleAll.shuffled = true;
            e.payload.state.ShuffleAll.vfxDone = true;
        }
        for (int i = 0; i < g.discard.size; ++i) {
            mark_card_move(g, g.discard[i]);
            g.draw.push_back(g.discard[i]);
            ++e.payload.state.ShuffleAll.count;
        }
        g.discard.clear();
    }
};
}
