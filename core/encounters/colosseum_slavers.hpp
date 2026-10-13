#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ColosseumSlavers {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::SlaverBlue), -270.0f);
        append_monster(g, construct_monster(g, MonsterId::SlaverRed), 130.0f);
    }
};
}
