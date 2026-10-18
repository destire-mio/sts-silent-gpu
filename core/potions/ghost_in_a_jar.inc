#pragma once
#include "core/hooks.hpp"

namespace spire::units::potions {
struct GhostInAJar : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    SPIRE_STATE(GhostInAJar, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.GhostInAJar.potency = data().potency * (has_relic(g, RelicId::SacredBark) ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].GhostInAJar.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        if (g.room_in_combat)
            queue_power(g, player_target, player_target, PowerId::IntangiblePlayer, potion_potency(g, slot));
    }
};
}
