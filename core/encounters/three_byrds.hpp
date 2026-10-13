#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct Byrds {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::Byrd), -360.0f);
        append_monster(g, construct_monster(g, MonsterId::Byrd), -80.0f);
        append_monster(g, construct_monster(g, MonsterId::Byrd), 200.0f);
    }
};
}
