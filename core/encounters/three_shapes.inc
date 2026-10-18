#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct ThreeShapes {
    SPIRE_FN static void spawn(Game& g, int count) {
        List<MonsterId, 6> pool{};
        pool.push_back(MonsterId::Repulsor); pool.push_back(MonsterId::Repulsor);
        pool.push_back(MonsterId::Exploder); pool.push_back(MonsterId::Exploder);
        pool.push_back(MonsterId::Spiker); pool.push_back(MonsterId::Spiker);
        for (int i = 0; i < count; ++i) {
            const int pick = g.rng.misc.random(pool.size - 1);
            const MonsterId id = pool[pick];
            pool.erase(pick);
            append_monster(g, construct_monster(g, id), -480.0f + 240.0f * static_cast<float>(i));
        }
    }
    SPIRE_FN static void build(Game& g) { spawn(g, 3); }
};
}
