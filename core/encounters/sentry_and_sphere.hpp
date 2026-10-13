#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct SentryAndSphere {
    SPIRE_FN static void build(Game& g) {
        Monster m0 = construct_monster(g, MonsterId::Sentry);
        position_monster(m0, -305.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::SphericGuardian);
        append_monster(g, m1);
    }
};
}
