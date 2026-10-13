#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct TwoFungiBeasts {
    SPIRE_FN static void build(Game& g) {
        append_monster(g, construct_monster(g, MonsterId::FungiBeast));
        append_monster(g, construct_monster(g, MonsterId::FungiBeast));
    }
};
}
