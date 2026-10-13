#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct NoxiousFumes : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) { p.amount += amount; }
    static SPIRE_FN void atStartOfTurnPostDraw(Game& g, Power& p, int owner) {
        if (monsters_basically_dead(g)) return;
        for (int i = 0; i < g.monster_order.size; ++i) {
            const int target = g.monster_order[i];
            const Creature& c = get_creature(g, target);
            if (!c.dead && !c.dying) queue_power(g, owner, target, PowerId::Poison, p.amount);
        }
    }
};
}
