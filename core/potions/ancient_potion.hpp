#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct AncientPotion : PotionHooks {
    SPIRE_STATE(AncientPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.AncientPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].AncientPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        if (g.room_in_combat)
            queue_power(g, player_target, player_target, PowerId::Artifact, potency(g, slot));
    }
};
}
