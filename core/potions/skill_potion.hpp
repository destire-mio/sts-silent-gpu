#pragma once
#include "../hooks.hpp"
#include "../actions/discovery.hpp"

namespace spire::units::potions {
struct SkillPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    SPIRE_STATE(SkillPotion, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.SkillPotion.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].SkillPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        add_bottom(g, units::actions::Discovery::construct(CardType::Skill, potion_potency(g, slot)));
    }
};
}
