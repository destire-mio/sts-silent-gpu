#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct DistilledChaosPotion : PotionHooks {
    SPIRE_STATE(DistilledChaosPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {3, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.DistilledChaosPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].DistilledChaosPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        for (int i = 0; i < potency(g, slot); ++i) {
            Effect e{};
            e.kind = EffectKind::PlayTopCardAction;
            e.target = random_living_monster(g);
            add_bottom(g, e);
        }
    }
};
}
