#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"

namespace spire::units::potions {
struct EnergyPotion : PotionHooks {
    SPIRE_STATE(EnergyPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {2, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.EnergyPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].EnergyPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        add_bottom(g, actions::GainEnergy::make(potency(g, slot)));
    }
};
}
