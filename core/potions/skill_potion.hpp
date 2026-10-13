#pragma once
#include "../hooks.hpp"
#include "../actions/discovery.hpp"

namespace spire::units::potions {
struct SkillPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        add_bottom(g, units::actions::Discovery::construct(CardType::Skill, potion_potency(g, slot)));
    }
};
}
