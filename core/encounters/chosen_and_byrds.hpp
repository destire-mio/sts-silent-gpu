#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ChosenAndByrds {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Byrd);
        position_monster(m0, -170.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::Chosen);
        position_monster(m1, 80.0f);
        append_monster(g, m1);
    }
};
}
