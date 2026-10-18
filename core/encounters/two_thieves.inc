#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct TwoThieves {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Looter);
        position_monster(m0, -200.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::Mugger);
        position_monster(m1, 80.0f);
        append_monster(g, m1);
    }
};
}
