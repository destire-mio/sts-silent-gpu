#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct TheMushroomLair {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::FungiBeast), -450.0f);
        append_monster(g, construct_monster(g, MonsterId::FungiBeast), -145.0f);
        append_monster(g, construct_monster(g, MonsterId::FungiBeast), 180.0f);
    }
};
}
