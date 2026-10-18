#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct GainEnergy : ActionHooks {
    SPIRE_STATE(GainEnergy, int energyGain;)
    SPIRE_FN static Effect make(int amount) {
        Effect e{};
        e.kind = EffectKind::GainEnergy;
        action_initialize_state(e);
        e.payload.state.GainEnergy.energyGain = amount;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        const int amount = e.payload.state.GainEnergy.energyGain;
        gain_energy(g, amount);
        g.manager.energy_gained_this_combat += amount;
        for (int i = next_card_triggerOnGainEnergy(g, g.hand, 0); i < g.hand.size; i = next_card_triggerOnGainEnergy(g, g.hand, i + 1)) card_triggerOnGainEnergy(g, g.hand[i], amount, true);
    }
};
}
