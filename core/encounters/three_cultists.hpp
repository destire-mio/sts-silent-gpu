#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ThreeCultists {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Cultist);
        position_monster(m0, -465.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::Cultist);
        position_monster(m1, -130.0f);
        append_monster(g, m1);
        Monster m2 = construct_monster(g, MonsterId::Cultist);
        position_monster(m2, 200.0f);
        append_monster(g, m2);
    }
};
}
