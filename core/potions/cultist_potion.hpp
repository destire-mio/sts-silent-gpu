#pragma once
#include "../hooks.hpp"
#include "../actions/intent_flash.hpp"

namespace spire::units::potions {
struct CultistPotion : PotionHooks {
    SPIRE_STATE(CultistPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {1, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.CultistPotion.potency = data().potency * (has_relic(g, RelicId::SacredBark) ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].CultistPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        if (g.room_in_combat) {
            Effect sound{};
            sound.kind = EffectKind::SFX;
            add_bottom(g, sound);
            add_bottom(g, actions::Talk::make(player_target));
            queue_power(g, player_target, player_target, PowerId::Ritual, potency(g, slot));
        }
    }
};
}
