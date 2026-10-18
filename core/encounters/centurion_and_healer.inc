#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct CenturionAndHealer {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Centurion);
        position_monster(m0, -200.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::Healer);
        position_monster(m1, 120.0f);
        append_monster(g, m1);
    }
};
}
