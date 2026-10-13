#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::potions {
struct SwiftPotion : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {3, CardTarget::Self, true}; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        add_bottom(g, units::actions::Draw::construct(potion_potency(g, slot), true, -1, player_target));
    }
};
}
