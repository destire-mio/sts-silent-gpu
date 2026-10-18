#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct LargeSlime {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, g.rng.misc.randomBoolean() ? MonsterId::AcidSlime_L : MonsterId::SpikeSlime_L));
    }
};
}
