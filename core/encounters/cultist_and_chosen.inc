#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct CultistAndChosen {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Cultist);
        position_monster(m0, -230.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::Chosen);
        position_monster(m1, 100.0f);
        append_monster(g, m1);
    }
};
}
