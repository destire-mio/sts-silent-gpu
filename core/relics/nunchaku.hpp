#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::relics {
struct Nunchaku : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {0, 150}; }
    static SPIRE_FN void onUseCard(Game& g, int relic, int card, Effect&) {
        if (card_data(g.cards[card].id).type != CardType::Attack) return;
        Relic& r = g.run.relics[relic];
        if (++r.counter % 10 != 0) return;
        r.counter = 0;
        queue_relic_above(g, relic, player_target);
        add_bottom(g, actions::GainEnergy::make(1));
    }
};
}
