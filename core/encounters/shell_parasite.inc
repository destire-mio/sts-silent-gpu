#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct ShellParasite {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::ShelledParasite);
        position_monster(m0, -20.0f);
        append_monster(g, m0);
    }
};
}
