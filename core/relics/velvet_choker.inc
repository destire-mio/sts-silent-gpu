#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct VelvetChoker : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN void onEquip(Game& g, int) { ++g.player.energy_master; }
    static SPIRE_FN void onUnequip(Game& g, int) { --g.player.energy_master; }
    static SPIRE_FN void atBattleStart(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void onPlayCard(Game& g, int relic, int, int) {
        if (g.run.relics[relic].counter < 6) ++g.run.relics[relic].counter;
    }
    static SPIRE_FN bool canPlay(const Game& g, int relic, int) { return g.run.relics[relic].counter < 6; }
    static SPIRE_FN void onVictory(Game& g, int relic) { g.run.relics[relic].counter = -1; }
};
}
