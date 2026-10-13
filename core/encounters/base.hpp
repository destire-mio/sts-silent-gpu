#pragma once
#include "../monsters/base.hpp"

namespace spire {
SPIRE_FN inline Monster random_louse(Game& g) {
    return construct_monster(g, g.rng.misc.randomBoolean() ? MonsterId::LouseNormal : MonsterId::LouseDefensive);
}

SPIRE_FN inline Monster weak_wildlife(Game& g) {
    // MonsterHelper.bottomGetWeakWildlife constructs every candidate before choosing.
    const Monster louse = random_louse(g);
    const Monster spike = construct_monster(g, MonsterId::SpikeSlime_M);
    const Monster acid = construct_monster(g, MonsterId::AcidSlime_M);
    const int pick = g.rng.misc.random(0, 2);
    return pick == 0 ? louse : pick == 1 ? spike : acid;
}

}
