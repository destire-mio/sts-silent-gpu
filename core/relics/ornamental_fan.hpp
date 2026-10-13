#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct OrnamentalFan : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void atTurnStart(Game& g, int relic) { g.run.relics[relic].counter = 0; }
    static SPIRE_FN void onUseCard(Game& g, int relic, int card, Effect&) {
        if (card_data(g.cards[card].id).type != CardType::Attack) return;
        Relic& r = g.run.relics[relic];
        if (++r.counter % 3 != 0) return;
        r.counter = 0;
        queue_relic_above(g, relic, player_target);
        queue_block(g, player_target, 4);
    }
    static SPIRE_FN void onVictory(Game& g, int relic) { g.run.relics[relic].counter = -1; }
};
}
