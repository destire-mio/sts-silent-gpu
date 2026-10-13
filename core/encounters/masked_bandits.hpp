#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct MaskedBandits {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::BanditPointy);
        position_monster(m0, -320.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::BanditLeader);
        position_monster(m1, -75.0f);
        append_monster(g, m1);
        Monster m2 = construct_monster(g, MonsterId::BanditBear);
        position_monster(m2, 150.0f);
        append_monster(g, m2);
    }
};
}
