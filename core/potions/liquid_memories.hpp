#pragma once
#include "../hooks.hpp"
#include "../actions/better_discard_pile_to_hand.hpp"

namespace spire::units::potions {
struct LiquidMemories : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        add_bottom(g, units::actions::BetterDiscardPileToHand::make(potion_potency(g, slot), 0));
    }
};
}
