#pragma once
#include "../hooks.hpp"
#include "../actions/discovery.hpp"

namespace spire::units::potions {
struct PowerPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    SPIRE_STATE(PowerPotion, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.PowerPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].PowerPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        add_bottom(g, units::actions::Discovery::construct(CardType::Power, potion_potency(g, slot)));
    }
};
}
