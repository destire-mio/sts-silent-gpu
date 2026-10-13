#pragma once
#include "../hooks.hpp"
#include "../actions/discovery.hpp"

namespace spire::units::potions {
struct ColorlessPotion : PotionHooks {
    SPIRE_STATE(ColorlessPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.ColorlessPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].ColorlessPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        add_bottom(g, actions::Discovery::construct(true, potency(g, slot)));
    }
};
}
