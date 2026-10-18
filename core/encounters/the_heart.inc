#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct TheHeart {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::CorruptHeart);
        position_monster(m0, -50.0f);
        append_monster(g, m0);
    }
};
}
