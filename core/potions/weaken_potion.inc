#pragma once
#include "core/hooks.hpp"

namespace spire::units::potions {
struct WeakenPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {3, CardTarget::Enemy, true}; }
    SPIRE_STATE(WeakenPotion, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.WeakenPotion.potency = data().potency * (has_relic(g, RelicId::SacredBark) ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].WeakenPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int target) {
        queue_power(g, player_target, target, PowerId::Weak, potion_potency(g, slot));
    }
};
}
