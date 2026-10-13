#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct MonsterStart : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_FN static void execute(Game& g, const Effect&) {
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int monster = g.monster_order[i];
            Creature& c = g.monsters[monster].creature;
            if (c.dying || c.escaped) continue;
            if (!keeps_block(g, monster)) c.block = 0;
            for (int p = 0; p < c.powers.size; ++p) power_atStartOfTurn(g, c.powers.at(g.power_objects, p), monster);
        }
    }
};
}
