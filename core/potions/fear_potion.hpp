#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct FearPotion : PotionHooks {
    SPIRE_STATE(FearPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {3, CardTarget::Enemy, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.FearPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].FearPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int target) {
        queue_power(g, player_target, target, PowerId::Vulnerable, potency(g, slot));
    }
};
}
