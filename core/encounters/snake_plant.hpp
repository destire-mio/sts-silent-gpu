#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct SnakePlant {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::SnakePlant);
        position_monster(m0, -30.0f);
        append_monster(g, m0);
    }
};
}
