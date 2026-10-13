#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct GremlinGang {
    SPIRE_FN static void build(Game& g) {
        List<MonsterId, 8> pool{};
        pool.push_back(MonsterId::GremlinWarrior);
        pool.push_back(MonsterId::GremlinWarrior);
        pool.push_back(MonsterId::GremlinThief);
        pool.push_back(MonsterId::GremlinThief);
        pool.push_back(MonsterId::GremlinFat);
        pool.push_back(MonsterId::GremlinFat);
        pool.push_back(MonsterId::GremlinTsundere);
        pool.push_back(MonsterId::GremlinWizard);
        for (int i = 0; i < 4; ++i) {
            const MonsterId id = pool.erase(g.rng.misc.random(pool.size - 1));
            append_monster(g, construct_monster(g, id));
        }
    }
};
}
