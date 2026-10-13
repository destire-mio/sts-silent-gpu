#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct RegenPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {5, CardTarget::Self, true}; }
    SPIRE_STATE(RegenPotion, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.RegenPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].RegenPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        if (g.room_in_combat)
            queue_power(g, player_target, player_target, PowerId::Regen, potion_potency(g, slot));
    }
};
}
