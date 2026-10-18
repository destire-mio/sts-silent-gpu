#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct Snecko {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Snecko);
        position_monster(m0, 0.0f);
        append_monster(g, m0);
    }
};
}
