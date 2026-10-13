#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct WeakenPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {3, CardTarget::Enemy, true}; }
    static SPIRE_FN void use(Game& g, int slot, int target) {
        queue_power(g, player_target, target, PowerId::Weak, potion_potency(g, slot));
    }
};
}
