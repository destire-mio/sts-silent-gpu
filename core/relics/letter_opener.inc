#pragma once
#include "core/hooks.hpp"
#include "core/actions/damage_all_enemies.hpp"

namespace spire::units::relics {
struct LetterOpener : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void onUseCard(Game& g, int relic, int card, Effect&) {
        if (card_data(g.cards[card].id).type == CardType::Skill) {
            int& counter = g.run.relics[relic].counter;
            if (++counter % 3 == 0) {
                counter = 0;
                queue_relic_above(g, relic, player_target, false);
                int amounts[monster_capacity]{};
                for (int i = 0; i < g.monster_order.size; ++i) amounts[i] = 5;
                add_bottom(g, units::actions::DamageAllEnemies::make(g, no_target, amounts, g.monster_order.size, DamageType::Thorns));
            }
        }
    }
    static SPIRE_FN void onVictory(Game& g, int relic) { g.run.relics[relic].counter = -1; }
};
}
