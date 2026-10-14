#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Shuriken : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void onUseCard(Game& g, int relic, int card, Effect&) {
        if (card_data(g.cards[card].id).type == CardType::Attack) {
            int& counter = g.run.relics[relic].counter;
            ++counter;
            if (counter % 3 == 0) {
                counter = 0;
                queue_relic_above(g, relic, player_target, false);
                queue_power(g, player_target, player_target, PowerId::Strength, 1);
            }
        }
    }
    static SPIRE_FN void onVictory(Game& g, int relic) { g.run.relics[relic].counter = -1; }
};
}
