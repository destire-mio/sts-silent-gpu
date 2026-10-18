#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct GainEnergyIfDiscard : ActionHooks {
    SPIRE_STATE(GainEnergyIfDiscard, int energyGain;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        const int amount = e.amount;
        action_initialize_state(e);
        e.source = e.target = player_target;
        e.amount = 0;
        e.payload.state.GainEnergyIfDiscard.energyGain = amount;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (g.manager.total_discarded_this_turn > 0) {
            const int amount = e.payload.state.GainEnergyIfDiscard.energyGain;
            gain_energy(g, amount);
            g.manager.energy_gained_this_combat += amount;
            for (int i = next_card_triggerOnGainEnergy(g, g.hand, 0); i < g.hand.size; i = next_card_triggerOnGainEnergy(g, g.hand, i + 1)) card_triggerOnGainEnergy(g, g.hand[i], amount, true);
        }
    }
};
}
