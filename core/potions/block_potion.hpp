#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct BlockPotion : PotionHooks {
    SPIRE_STATE(BlockPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {12, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.BlockPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].BlockPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        queue_block(g, player_target, potency(g, slot));
    }
};
}
