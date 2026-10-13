#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct FruitJuice : PotionHooks {
    static SPIRE_FN constexpr PotionData data() { return {5, CardTarget::Self, true}; }
    SPIRE_STATE(FruitJuice, int potency;)
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.FruitJuice.potency = data().potency * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].FruitJuice.potency; }
    static SPIRE_FN bool canUse(const Game& g, int slot) {
        if (g.manager.turn_has_ended && g.room_in_combat) return false;
        return potion_can_discard_base(g, slot);
    }
    static SPIRE_FN void use(Game& g, int slot, int) {
        g.player.creature.max_hp += potion_potency(g, slot);
        heal_creature(g, player_target, potion_potency(g, slot));
    }
};
}
