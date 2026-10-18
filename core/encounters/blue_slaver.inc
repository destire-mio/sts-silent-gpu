#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct BlueSlaver {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::SlaverBlue));
    }
};
}
