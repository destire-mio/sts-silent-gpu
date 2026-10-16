#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct EssenceOfSteel : PotionHooks {
    SPIRE_STATE(EssenceOfSteel, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {4, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.EssenceOfSteel.potency = data().potency * (has_relic(g, RelicId::SacredBark) ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].EssenceOfSteel.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        if (g.room_in_combat)
            queue_power(g, player_target, player_target, PowerId::PlatedArmor, potency(g, slot));
    }
};
}
