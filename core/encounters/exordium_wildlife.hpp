#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ExordiumWildlife {
    SPIRE_FN static void build(Game& g) {
        float x = -150.0f + g.unseeded.random(-20.0f, 20.0f);
        // MonsterHelper.bottomGetStrongWildlife constructs both before choosing.
        const Monster fungi = construct_monster(g, MonsterId::FungiBeast);
        const Monster jaw = construct_monster(g, MonsterId::JawWorm);
        append_monster(g, g.rng.misc.random(0, 1) == 0 ? fungi : jaw, x);
        x = 150.0f + g.unseeded.random(-20.0f, 20.0f);
        append_monster(g, weak_wildlife(g), x);
    }
};
}
