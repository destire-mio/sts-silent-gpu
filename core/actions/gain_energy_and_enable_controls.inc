#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct GainEnergyAndEnableControls : ActionHooks {
    SPIRE_STATE(GainEnergyAndEnableControls, int energyGain;)
    SPIRE_FN static Effect make(int amount) {
        Effect e{};
        e.kind = EffectKind::GainEnergyAndEnableControls;
        action_initialize_state(e);
        e.payload.state.GainEnergyAndEnableControls.energyGain = amount;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        const int amount = e.payload.state.GainEnergyAndEnableControls.energyGain;
        gain_energy(g, amount);
        g.manager.energy_gained_this_combat += amount;
        for (int i = next_card_triggerOnGainEnergy(g, g.hand, 0); i < g.hand.size; i = next_card_triggerOnGainEnergy(g, g.hand, i + 1)) card_triggerOnGainEnergy(g, g.hand[i], amount, false);
        for (int i = next_relic_onEnergyRecharge(g, 0); i < g.run.relics.size; i = next_relic_onEnergyRecharge(g, i + 1)) relic_onEnergyRecharge(g, i);
        Powers& powers = g.player.creature.powers;
        for (int p = next_power_onEnergyRecharge(g, powers.head); p != 255; p = next_power_onEnergyRecharge(g, g.power_objects[p].next)) power_onEnergyRecharge(g, g.power_objects[p], player_target);
        g.manager.turn_has_ended = false;
    }
};
}
