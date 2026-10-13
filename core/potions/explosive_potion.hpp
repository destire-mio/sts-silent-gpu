#pragma once
#include "../hooks.hpp"
#include "../actions/damage_all_enemies.hpp"

namespace spire::units::potions {
struct ExplosivePotion : PotionHooks {
    SPIRE_STATE(ExplosivePotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {10, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.ExplosivePotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].ExplosivePotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        int amounts[monster_capacity];
        for (int i = 0; i < g.monster_order.size; ++i) amounts[i] = potency(g, slot);
        add_bottom(g, actions::DamageAllEnemies::make(g, no_target, amounts, g.monster_order.size));
    }
};
}
