#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct Darkling {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::Darkling), -440.0f);
        append_monster(g, construct_monster(g, MonsterId::Darkling), -140.0f);
        append_monster(g, construct_monster(g, MonsterId::Darkling), 180.0f);
    }
};
}
