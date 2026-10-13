#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct TwoLouse {
    SPIRE_FN static void build(Game& g) {
        for (int i = 0; i < 2; ++i) append_monster(g, random_louse(g));
    }
};
}
