#pragma once
#include "base.hpp"

namespace spire::units::encounters {
struct ShieldAndSpear {
    SPIRE_FN static void build(Game& g) {
        // AbstractDungeon.nextRoomTransition centers the player for this encounter.
        g.player.creature.draw_x = 1280.0f / 2.0f;
        Monster m0 = construct_monster(g, MonsterId::SpireShield);
        position_monster(m0, -1000.0f);
        append_monster(g, m0);
        Monster m1 = construct_monster(g, MonsterId::SpireSpear);
        position_monster(m1, 70.0f);
        append_monster(g, m1);
    }
};
}
