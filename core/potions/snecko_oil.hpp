#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::potions {
struct SneckoOil : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {5, CardTarget::Self, true}; }
    SPIRE_STATE(SneckoOil, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.SneckoOil.potency = data().potency * (has_relic(g, RelicId::SacredBark) ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].SneckoOil.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        if (g.room_in_combat) {
            add_bottom(g, units::actions::Draw::construct(potion_potency(g, slot), true, -1, player_target));
            Effect e{};
            e.kind = EffectKind::RandomizeHandCost;
            add_bottom(g, e);
        }
    }
};
}
