#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::potions {
struct SwiftPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {3, CardTarget::Self, true}; }
    SPIRE_STATE(SwiftPotion, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.SwiftPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].SwiftPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        add_bottom(g, units::actions::Draw::construct(potion_potency(g, slot), true, -1, player_target));
    }
};
}
