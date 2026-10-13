#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct SlimeBoss {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::SlimeBoss), 0.0f);
    }
};
}
