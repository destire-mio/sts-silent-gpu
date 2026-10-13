#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct GremlinLeader {
    SPIRE_FN static void build(Game& g) {
        constexpr MonsterId pool[]{MonsterId::GremlinWarrior, MonsterId::GremlinWarrior,
            MonsterId::GremlinThief, MonsterId::GremlinThief, MonsterId::GremlinFat,
            MonsterId::GremlinFat, MonsterId::GremlinTsundere, MonsterId::GremlinWizard};
        append_monster(g, construct_monster(g, pool[g.rng.misc.random(0, 7)]), -366.0f);
        append_monster(g, construct_monster(g, pool[g.rng.misc.random(0, 7)]), -170.0f);
        append_monster(g, construct_monster(g, MonsterId::GremlinLeader));
    }
};
}
