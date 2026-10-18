#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct MushroomLair {
    SPIRE_FN static void build(Game& g) {
        {
            Monster m = construct_monster(g, MonsterId::FungiBeast);
            append_monster(g, m, -450.0f);
        }
        {
            Monster m = construct_monster(g, MonsterId::FungiBeast);
            append_monster(g, m, -145.0f);
        }
        {
            Monster m = construct_monster(g, MonsterId::FungiBeast);
            append_monster(g, m, 180.0f);
        }
    }
};
}
