#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct SpeedPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {5, CardTarget::Self, true}; }
    SPIRE_STATE(SpeedPotion, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.SpeedPotion.potency = data().potency * (has_relic(g, RelicId::SacredBark) ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].SpeedPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        if (g.room_in_combat) {
            queue_power(g, player_target, player_target, PowerId::Dexterity, potion_potency(g, slot));
            queue_power(g, player_target, player_target, PowerId::LoseDexterity, potion_potency(g, slot));
        }
    }
};
}
