#pragma once
#include "../hooks.hpp"
#include "../actions/discovery.hpp"

namespace spire::units::potions {
struct AttackPotion : PotionHooks {
    SPIRE_STATE(AttackPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.AttackPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].AttackPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        add_bottom(g, actions::Discovery::construct(CardType::Attack, potency(g, slot)));
    }
};
}
