#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct GainEnergyAndEnableControls : ActionHooks {
    SPIRE_STATE(GainEnergyAndEnableControls, int energyGain;)
    SPIRE_FN static Effect make(int amount) {
        Effect e{};
        e.kind = EffectKind::GainEnergyAndEnableControls;
        action_initialize_state(e);
        e.state.GainEnergyAndEnableControls.energyGain = amount;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        const int amount = e.state.GainEnergyAndEnableControls.energyGain;
        gain_energy(g, amount);
        g.manager.energy_gained_this_combat += amount;
        for (int i = 0; i < g.hand.size; ++i) card_triggerOnGainEnergy(g, g.hand[i], amount, false);
        for (int i = 0; i < g.run.relics.size; ++i) relic_onEnergyRecharge(g, i);
        Powers& powers = g.player.creature.powers;
        for (int i = 0; i < powers.size; ++i) power_onEnergyRecharge(g, powers[i], player_target);
        g.manager.turn_has_ended = false;
    }
};
}
