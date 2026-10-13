#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Abacus : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onShuffle(Game& g, int relic) {
        queue_relic_display(g, relic, player_target, false);
        // EmptyDeckShuffleAction constructor calls Abacus.onShuffle before it is queued.
        Effect block{};
        block.kind = EffectKind::Block;
        block.amount = 6;
        action_initialize_state(block);
        g.manager.actions.push_back(g.action_objects.add(block));
    }
};
}

namespace spire::units::actions {
struct RelicAboveCreature : ActionHooks {
    SPIRE_SNAPSHOT("RelicAboveCreatureAction.relic", relic, effect.extra)
    SPIRE_STATE(RelicAboveCreature, bool used;)
    static SPIRE_FN constexpr ActionData data() { return {false, "TEXT"}; }
    static SPIRE_FN void execute(Game&, Effect& e) { e.state.RelicAboveCreature.used = true; }
};
}
namespace spire {
SPIRE_FN inline void queue_relic_display(Game& g, int relic, int target, bool top) {
    Effect e{};
    e.kind = EffectKind::RelicAboveCreature;
    e.source = e.target = target;
    e.extra = relic;
    action_initialize_state(e);
    const int index = g.action_objects.add(e);
    if (top) g.manager.actions.push_front(index);
    else g.manager.actions.push_back(index);
}
}
