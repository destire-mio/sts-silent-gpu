#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct JawWormHorde {
    SPIRE_FN static void build(Game& g) {
        {
            Monster m = construct_monster(g, MonsterId::JawWorm);
            m.state.JawWorm.hardMode = true; m.state.JawWorm.firstMove = false;
            append_monster(g, m, -490.0f);
        }
        {
            Monster m = construct_monster(g, MonsterId::JawWorm);
            m.state.JawWorm.hardMode = true; m.state.JawWorm.firstMove = false;
            append_monster(g, m, -150.0f);
        }
        {
            Monster m = construct_monster(g, MonsterId::JawWorm);
            m.state.JawWorm.hardMode = true; m.state.JawWorm.firstMove = false;
            append_monster(g, m, 175.0f);
        }
    }
};
}
