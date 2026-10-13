#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct LotsOfSlimes {
    SPIRE_FN static void build(Game& g) {
        List<MonsterId, 5> pool{};
        pool.push_back(MonsterId::SpikeSlime_S);
        pool.push_back(MonsterId::SpikeSlime_S);
        pool.push_back(MonsterId::SpikeSlime_S);
        pool.push_back(MonsterId::AcidSlime_S);
        pool.push_back(MonsterId::AcidSlime_S);
        while (!pool.empty()) {
            const MonsterId id = pool.erase(g.rng.misc.random(pool.size - 1));
            append_monster(g, construct_monster(g, id));
        }
    }
};
}
