#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct PreservedInsect : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    SPIRE_STATE(PreservedInsect, float MODIFIER_AMT;)
    static SPIRE_FN void initializeState(Relic& r) { r.state.PreservedInsect.MODIFIER_AMT = 0.25f; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        if (!g.room_elite_trigger) return;
        for (int i = 0; i < g.monster_order.size; ++i) {
            Creature& m = g.monsters[g.monster_order[i]].creature;
            const int hp = static_cast<int>(static_cast<float>(m.max_hp) * (1.0f - g.run.relics[relic].state.PreservedInsect.MODIFIER_AMT));
            if (m.hp > hp) m.hp = hp;
        }
        queue_relic_above(g, relic, player_target, true);
    }
    static SPIRE_FN bool canSpawn(const Game& g, int) { return g.run.floor <= 52; }
};
}
