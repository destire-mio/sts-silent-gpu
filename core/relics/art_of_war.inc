#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct ArtOfWar : RelicHooks {
    SPIRE_STATE(ArtOfWar, bool gainEnergyNext; bool firstTurn;)
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void atPreBattle(Game& g, int relic) {
        auto& state = g.run.relics[relic].state.ArtOfWar;
        state.firstTurn = true;
        state.gainEnergyNext = true;
    }
    static SPIRE_FN void atTurnStart(Game& g, int relic) {
        auto& state = g.run.relics[relic].state.ArtOfWar;
        if (state.gainEnergyNext && !state.firstTurn) {
            queue_relic_above(g, relic, player_target, false);
            Effect e = units::actions::GainEnergy::make(1);
            add_bottom(g, e);
        }
        state.firstTurn = false;
        state.gainEnergyNext = true;
    }
    static SPIRE_FN void onUseCard(Game& g, int relic, int card, Effect&) {
        if (card_def(g.cards[card].id).type == CardType::Attack) g.run.relics[relic].state.ArtOfWar.gainEnergyNext = false;
    }
    static SPIRE_FN void onVictory(Game&, int) {}
};
}
