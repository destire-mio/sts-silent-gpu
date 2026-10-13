#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct LagavulinEvent {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Lagavulin);
        m0.state.Lagavulin.asleep = false;
        m0.state.Lagavulin.isOut = true;
        m0.state.Lagavulin.isOutTriggered = true;
        append_monster(g, m0, 0.0f);
    }
};
}
