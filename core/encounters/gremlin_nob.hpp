#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct GremlinNob {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::GremlinNob);
        position_monster(m0, 0.0f);
        append_monster(g, m0);
    }
};
}
