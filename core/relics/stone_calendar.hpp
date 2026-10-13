#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::relics {
struct StoneCalendar : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    SPIRE_STATE(StoneCalendar, bool grayscale;)
    SPIRE_SNAPSHOT("AbstractRelic.grayscale", scalar, relic.state.StoneCalendar.grayscale)
    static SPIRE_FN void atBattleStart(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) { ++g.run.relics[relic].counter; }
    static SPIRE_FN void onPlayerEndTurn(Game& g, int relic) {
        if (g.run.relics[relic].counter == 7) {
            int amounts[monster_capacity]{};
            for (int i = 0; i < g.monster_order.size; ++i) amounts[i] = 52;
            add_bottom(g, actions::DamageAllEnemies::make(no_target, amounts, g.monster_order.size, DamageType::Thorns));
            g.run.relics[relic].state.StoneCalendar.grayscale = true;
        }
    }
    static SPIRE_FN void justEnteredRoom(Game& g, int relic, int) {
        g.run.relics[relic].state.StoneCalendar.grayscale = false;
    }
    static SPIRE_FN void onVictory(Game& g, int relic) { g.run.relics[relic].counter = -1; }
};
}
