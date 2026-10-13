#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct GainEnergy : ActionHooks {
    SPIRE_STATE(GainEnergy, int energyGain;)
    SPIRE_FN static Effect make(int amount) {
        Effect e{};
        e.kind = EffectKind::GainEnergy;
        action_initialize_state(e);
        e.state.GainEnergy.energyGain = amount;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        const int amount = e.state.GainEnergy.energyGain;
        gain_energy(g, amount);
        g.manager.energy_gained_this_combat += amount;
        for (int i = 0; i < g.hand.size; ++i) card_triggerOnGainEnergy(g, g.hand[i], amount, true);
    }
};
}
