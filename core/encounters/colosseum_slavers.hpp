#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ColosseumSlavers {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::SlaverBlue);
        position_monster(m0, -270.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::SlaverRed);
        position_monster(m1, 130.0f);
        append_monster(g, m1);
    }
};
}
