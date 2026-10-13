#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct OrbWalker {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::OrbWalker), -30.0f);
    }
};
}
