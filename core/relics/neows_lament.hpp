#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct NeowsLament : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {3, 400}; }
    static SPIRE_FN void setCounter(Game& g, int relic, int amount) {
        Relic& r = g.run.relics[relic];
        r.counter = amount;
        if (amount <= 0) r.grayscale = r.used_up = true;
    }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        Relic& r = g.run.relics[relic];
        if (r.counter <= 0) return;
        if (--r.counter == 0) setCounter(g, relic, -2);
        for (int i = 0; i < g.monster_order.size; ++i) g.monsters[g.monster_order[i]].creature.hp = 1;
        queue_relic_above(g, relic, player_target, true);
    }
};
}
