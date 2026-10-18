#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct AwakenedOne {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Cultist);
        m0.state.Cultist.talky = false;
        append_monster(g, m0, -590.0f);
        Monster m1 = construct_monster(g, MonsterId::Cultist);
        m1.state.Cultist.talky = false;
        append_monster(g, m1, -298.0f);
        append_monster(g, construct_monster(g, MonsterId::AwakenedOne), 100.0f);
    }
};
}
