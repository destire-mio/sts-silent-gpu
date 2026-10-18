#pragma once
#include "core/encounters/base.hpp"

namespace spire::units::encounters {
struct ExordiumThugs {
    SPIRE_FN static void build(Game& g) {
        float x = -160.0f + g.unseeded.random(-20.0f, 20.0f);
        append_monster(g, weak_wildlife(g), x);
        x = 130.0f + g.unseeded.random(-20.0f, 20.0f);
        // MonsterHelper.bottomGetStrongHumanoid consumes discarded candidates' HP rolls.
        const Monster cultist = construct_monster(g, MonsterId::Cultist);
        const Monster slaver = construct_monster(g, g.rng.misc.randomBoolean() ? MonsterId::SlaverRed : MonsterId::SlaverBlue);
        const Monster looter = construct_monster(g, MonsterId::Looter);
        const int pick = g.rng.misc.random(0, 2);
        append_monster(g, pick == 0 ? cultist : pick == 1 ? slaver : looter, x);
    }
};
}
