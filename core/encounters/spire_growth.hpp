#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct SpireGrowth {
    SPIRE_FN static void build(Game& g) { append_monster(g, construct_monster(g, MonsterId::SpireGrowth)); }
};
}
