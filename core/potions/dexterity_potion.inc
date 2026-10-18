#pragma once
#include "core/hooks.hpp"

namespace spire::units::potions {
struct DexterityPotion : PotionHooks {
    SPIRE_STATE(DexterityPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {2, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.DexterityPotion.potency = data().potency * (has_relic(g, RelicId::SacredBark) ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].DexterityPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        queue_power(g, player_target, player_target, PowerId::Dexterity, potency(g, slot));
    }
};
}
