#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct BronzeAutomaton {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::BronzeAutomaton), 0.0f);
    }
};
}
