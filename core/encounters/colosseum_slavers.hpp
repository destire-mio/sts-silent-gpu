#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ColosseumSlavers {
    SPIRE_FN static void build(Game& g) {
        {
            Monster m = construct_monster(g, MonsterId::SlaverBlue);
            append_monster(g, m, -270.0f);
        }
        {
            Monster m = construct_monster(g, MonsterId::SlaverRed);
            append_monster(g, m, 130.0f);
        }
    }
};
}
