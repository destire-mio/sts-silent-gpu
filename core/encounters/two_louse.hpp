#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct TwoLouse {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, random_louse(g), -200.0f, 10.0f);
        append_monster(g, random_louse(g), 80.0f, 30.0f);
    }
};
}
