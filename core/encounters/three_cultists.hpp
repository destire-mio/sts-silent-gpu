#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ThreeCultists {
    SPIRE_FN static void build(Game& g) {
        {
            Monster m = construct_monster(g, MonsterId::Cultist);
            append_monster(g, m, -465.0f);
        }
        {
            Monster m = construct_monster(g, MonsterId::Cultist);
            append_monster(g, m, -130.0f);
        }
        {
            Monster m = construct_monster(g, MonsterId::Cultist);
            append_monster(g, m, 200.0f);
        }
    }
};
}
