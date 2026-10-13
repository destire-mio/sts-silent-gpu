#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Abacus : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onShuffle(Game& g, int relic) {
        queue_relic_above(g, relic, player_target, false);
        // EmptyDeckShuffleAction constructor calls Abacus.onShuffle before it is queued.
        Effect block{};
        block.kind = EffectKind::Block;
        block.amount = 6;
        action_initialize_state(block);
        g.manager.actions.push_back(g.action_objects.add(block));
    }
};
}
