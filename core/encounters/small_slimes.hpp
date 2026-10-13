#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct SmallSlimes {
    SPIRE_FN static void build(Game& g) {
        if (g.rng.misc.randomBoolean()) {
            append_monster(g, construct_monster(g, MonsterId::SpikeSlime_S), -230.0f);
            append_monster(g, construct_monster(g, MonsterId::AcidSlime_M), 35.0f);
        } else {
            append_monster(g, construct_monster(g, MonsterId::AcidSlime_S), -230.0f);
            append_monster(g, construct_monster(g, MonsterId::SpikeSlime_M), 35.0f);
        }
    }
};
}
