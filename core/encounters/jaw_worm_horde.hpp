#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct JawWormHorde {
    SPIRE_FN static void build(Game& g) {
        constexpr float positions[]{-490.0f, -150.0f, 175.0f};
        for (int i = 0; i < 3; ++i) {
            Monster m = construct_monster(g, MonsterId::JawWorm);
            m.state.JawWorm.hardMode = true; m.state.JawWorm.firstMove = false;
            append_monster(g, m, positions[i]);
        }
    }
};
}
