#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct FirePotion : PotionHooks {
    SPIRE_STATE(FirePotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {20, CardTarget::Enemy, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.FirePotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].FirePotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int target) {
        const int base = potency(g, slot);
        float amount = static_cast<float>(base);
        const Powers& powers = get_creature(g, target).powers;
        // DamageInfo.applyEnemyPowersOnly overwrites tmp using output in each hook.
        for (int i = 0; i < powers.size; ++i)
            amount = power_atDamageReceive(g, powers.at(g.power_objects, i), target, static_cast<float>(base), DamageType::Thorns);
        for (int i = 0; i < powers.size; ++i)
            amount = power_atDamageFinalReceive(g, powers.at(g.power_objects, i), target, static_cast<float>(base), DamageType::Thorns);
        Effect e{};
        e.kind = EffectKind::Damage;
        e.source = player_target;
        e.target = target;
        e.info_base = base;
        e.amount = floor_int(amount < 0.0f ? 0.0f : amount);
        e.damage_type = DamageType::Thorns;
        add_bottom(g, e);
    }
};
}
