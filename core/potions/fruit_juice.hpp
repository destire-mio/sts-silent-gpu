#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct FruitJuice : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {5, CardTarget::Self, true}; }
    static SPIRE_FN bool canUse(const Game& g, int slot) {
        if (g.manager.turn_has_ended && !g.battle_over && g.screen != Screen::None) return false;
        return potion_can_discard_base(g, slot);
    }
    static SPIRE_FN void use(Game& g, int slot, int) {
        g.player.creature.max_hp += potion_potency(g, slot);
        heal_creature(g, player_target, potion_potency(g, slot));
    }
};
}
