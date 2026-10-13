#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct DuplicationPotion : PotionHooks {
    SPIRE_STATE(DuplicationPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.DuplicationPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].DuplicationPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        if (g.room_in_combat)
            queue_power(g, player_target, player_target, PowerId::Duplication, potency(g, slot));
    }
};
}
