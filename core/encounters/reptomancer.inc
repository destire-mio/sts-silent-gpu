#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct Reptomancer {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::SnakeDagger);
        position_monster(m0, -220.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::Reptomancer);
        append_monster(g, m1);
        Monster m2 = construct_monster(g, MonsterId::SnakeDagger);
        position_monster(m2, 210.0f);
        append_monster(g, m2);
    }
};
}
