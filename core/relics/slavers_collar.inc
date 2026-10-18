#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct SlaversCollar : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    SPIRE_STATE(SlaversCollar, bool pulse;)
    static SPIRE_FN void beforeEnergyPrep(Game& g, int relic) {
        bool elite_or_boss = g.room_elite_trigger;
        for (int i = 0; i < g.monster_order.size; ++i)
            if (monster_data(g.monsters[g.monster_order[i]].id).type == EnemyType::Boss) elite_or_boss = true;
        if (elite_or_boss) {
            g.run.relics[relic].state.SlaversCollar.pulse = true;
            ++g.player.energy_master;
        }
    }
    static SPIRE_FN void onVictory(Game& g, int relic) {
        bool& pulse = g.run.relics[relic].state.SlaversCollar.pulse;
        if (pulse) {
            --g.player.energy_master;
            pulse = false;
        }
    }
};
}
