#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct PenNib : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {0, 150}; }
    static SPIRE_FN void onUseCard(Game& g, int relic, int card, Effect&) {
        if (card_data(g.cards[card].id).type != CardType::Attack) return;
        Relic& r = g.run.relics[relic];
        ++r.counter;
        if (r.counter == 10) r.counter = 0;
        else if (r.counter == 9) {
            queue_relic_above(g, relic, player_target);
            queue_power(g, player_target, player_target, PowerId::PenNib, 1);
        }
    }
    static SPIRE_FN void atBattleStart(Game& g, int relic) {
        if (g.run.relics[relic].counter == 9) queue_power(g, player_target, player_target, PowerId::PenNib, 1);
    }
};
}
