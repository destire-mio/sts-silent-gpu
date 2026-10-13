#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ColosseumNobs {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Taskmaster);
        position_monster(m0, -270.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::GremlinNob);
        position_monster(m1, 130.0f);
        append_monster(g, m1);
    }
};
}
