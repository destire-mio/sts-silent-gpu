#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct StrengthPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {2, CardTarget::Self, true}; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        if (!g.battle_over && g.screen != Screen::None)
            queue_power(g, player_target, player_target, PowerId::Strength, potion_potency(g, slot));
    }
};
}
