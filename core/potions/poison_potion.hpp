#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct PoisonPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {6, CardTarget::Enemy, true}; }
    SPIRE_STATE(PoisonPotion, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.PoisonPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].PoisonPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int target) {
        queue_power(g, player_target, target, PowerId::Poison, potion_potency(g, slot));
    }
};
}
