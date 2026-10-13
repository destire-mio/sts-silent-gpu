#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ThreeLouse {
    SPIRE_FN static void build(Game& g) {
        constexpr float positions[3][2]{{-350.0f, 25.0f}, {-125.0f, 10.0f}, {80.0f, 30.0f}};
        for (int i = 0; i < 3; ++i) append_monster(g, random_louse(g), positions[i][0], positions[i][1]);
    }
};
}
