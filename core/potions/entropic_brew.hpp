#pragma once
#include "../hooks.hpp"
#include "../actions/obtain_potion.hpp"

namespace spire::units::potions {
struct EntropicBrew : PotionHooks {
    SPIRE_STATE(EntropicBrew, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {2, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.EntropicBrew.potency = g.run.potion_slots * (has_relic(g, "SacredBark") ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].EntropicBrew.potency; }
    static SPIRE_FN bool canUse(const Game& g, int slot) { return PotionHooks::canDiscard(g, slot) && !(g.manager.turn_has_ended && g.room_in_combat); }
    static SPIRE_FN void use(Game& g, int, int) {
        if (g.room_in_combat) {
            for (int i = 0; i < g.run.potion_slots; ++i) {
                const PotionId id = random_potion(g, true);
                add_bottom(g, actions::ObtainPotion::from_potion(id, construct_potion(g, id)));
            }
        } else if (!has_relic(g, "Sozu")) {
            for (int i = 0; i < g.run.potion_slots; ++i) {
                const PotionId id = random_potion(g, false);
                obtain_potion(g, id, construct_potion(g, id));
            }
        }
    }
};
}
