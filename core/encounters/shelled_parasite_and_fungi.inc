#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct ShelledParasiteAndFungi {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::ShelledParasite);
        position_monster(m0, -260.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::FungiBeast);
        position_monster(m1, 120.0f);
        append_monster(g, m1);
    }
};
}
