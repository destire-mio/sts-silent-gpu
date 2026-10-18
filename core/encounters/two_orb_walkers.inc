#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct TwoOrbWalkers {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::OrbWalker), -250.0f);
        append_monster(g, construct_monster(g, MonsterId::OrbWalker), 150.0f);
    }
};
}
