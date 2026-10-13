#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct BlessingOfTheForge : PotionHooks {
    SPIRE_STATE(BlessingOfTheForge, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {0, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.BlessingOfTheForge.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].BlessingOfTheForge.potency; }
    static SPIRE_FN void use(Game& g, int, int) {
        if (g.room_in_combat) {
            Effect e{};
            e.kind = EffectKind::Armaments;
            action_initialize_state(e);
            e.state.Armaments.upgraded = true;
            add_bottom(g, e);
        }
    }
};
}
