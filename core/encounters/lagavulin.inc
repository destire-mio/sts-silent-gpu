#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct Lagavulin {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::Lagavulin), 0.0f);
    }
};
}
