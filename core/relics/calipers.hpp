#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Calipers : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
};
}

namespace spire {
SPIRE_FN inline int relic_block_loss(const Game& g) {
    return has_relic(g, "Calipers") ? 15 : g.player.creature.block;
}
}
