#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct SmallSlimes {
    SPIRE_FN static void build(Game& g) {
        if (g.rng.misc.randomBoolean()) {
            append_monster(g, construct_monster(g, MonsterId::SpikeSlime_S));
            append_monster(g, construct_monster(g, MonsterId::AcidSlime_M));
        } else {
            append_monster(g, construct_monster(g, MonsterId::AcidSlime_S));
            append_monster(g, construct_monster(g, MonsterId::SpikeSlime_M));
        }
    }
};
}
