#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct ThreeCultists {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Cultist);
        m0.state.Cultist.talky = false;
        append_monster(g, m0, -465.0f);
        Monster m1 = construct_monster(g, MonsterId::Cultist);
        m1.state.Cultist.talky = false;
        append_monster(g, m1, -130.0f);
        Monster m2 = construct_monster(g, MonsterId::Cultist);
        append_monster(g, m2, 200.0f);
    }
};
}
