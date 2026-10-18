#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct Slavers {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::SlaverBlue);
        position_monster(m0, -385.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::Taskmaster);
        position_monster(m1, -133.0f);
        append_monster(g, m1);
        Monster m2 = construct_monster(g, MonsterId::SlaverRed);
        position_monster(m2, 125.0f);
        append_monster(g, m2);
    }
};
}
