#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct JawWormHorde {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::JawWorm);
        m0.state.JawWorm.hardMode = true;
        m0.state.JawWorm.firstMove = false;
        append_monster(g, m0, -490.0f);
        Monster m1 = construct_monster(g, MonsterId::JawWorm);
        m1.state.JawWorm.hardMode = true;
        m1.state.JawWorm.firstMove = false;
        append_monster(g, m1, -150.0f);
        Monster m2 = construct_monster(g, MonsterId::JawWorm);
        m2.state.JawWorm.hardMode = true;
        m2.state.JawWorm.firstMove = false;
        append_monster(g, m2, 175.0f);
    }
};
}
