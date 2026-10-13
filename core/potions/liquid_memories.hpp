#pragma once
#include "../hooks.hpp"
#include "../actions/better_discard_pile_to_hand.hpp"

namespace spire::units::potions {
struct LiquidMemories : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    SPIRE_STATE(LiquidMemories, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.LiquidMemories.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].LiquidMemories.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        add_bottom(g, units::actions::BetterDiscardPileToHand::make(potion_potency(g, slot), 0));
    }
};
}
