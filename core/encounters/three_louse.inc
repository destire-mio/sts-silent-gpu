#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct ThreeLouse {
    SPIRE_FN static void build(Game& g) {
        constexpr float positions[3]{-350.0f, -125.0f, 80.0f};
        for (int i = 0; i < 3; ++i) append_monster(g, random_louse(g), positions[i]);
    }
};
}
