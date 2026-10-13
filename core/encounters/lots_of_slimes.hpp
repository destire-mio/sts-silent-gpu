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
        constexpr float positions[5][2]{{-480.0f, 30.0f}, {-320.0f, 2.0f}, {-160.0f, 32.0f}, {10.0f, -12.0f}, {200.0f, 9.0f}};
        for (int i = 0; i < 5; ++i) {
            const MonsterId id = pool.erase(g.rng.misc.random(pool.size - 1));
            append_monster(g, construct_monster(g, id), positions[i][0], positions[i][1]);
        }
    }
};
}
