#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct SphereAndTwoShapes {
    SPIRE_FN static void build(Game& g) {
        constexpr MonsterId shapes[]{MonsterId::Spiker, MonsterId::Repulsor, MonsterId::Exploder};
        for (int i = 0; i < 2; ++i) {
            Monster m = construct_monster(g, shapes[g.rng.misc.random(2)]);
            position_monster(m, i == 0 ? -435.0f : -210.0f);
            append_monster(g, m);
        }
        Monster m0 = construct_monster(g, MonsterId::SphericGuardian);
        position_monster(m0, 110.0f);
        append_monster(g, m0);
    }
};
}
