#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct PoisonPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {6, CardTarget::Enemy, true}; }
    static SPIRE_FN void use(Game& g, int slot, int target) {
        queue_power(g, player_target, target, PowerId::Poison, potion_potency(g, slot));
    }
};
}
