#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct FairyPotion : PotionHooks {
    SPIRE_STATE(FairyPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {30, CardTarget::Self, true, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.FairyPotion.potency = data().potency * (has_relic(g, RelicId::SacredBark) ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].FairyPotion.potency; }
    static SPIRE_FN bool canUse(const Game&, int) { return false; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        const float percent = static_cast<float>(potency(g, slot)) / 100.0f;
        const int amount = max(1, static_cast<int>(static_cast<float>(g.player.creature.max_hp) * percent));
        heal_creature(g, player_target, amount);
        g.run.potions[slot] = PotionId::Empty;
    }
};
}
