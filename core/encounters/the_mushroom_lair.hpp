#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct TheMushroomLair {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::FungiBeast);
        position_monster(m0, -450.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::FungiBeast);
        position_monster(m1, -145.0f);
        append_monster(g, m1);
        Monster m2 = construct_monster(g, MonsterId::FungiBeast);
        position_monster(m2, 180.0f);
        append_monster(g, m2);
    }
};
}
