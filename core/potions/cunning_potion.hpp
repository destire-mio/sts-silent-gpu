#pragma once
#include "../hooks.hpp"

namespace spire::units::potions {
struct CunningPotion : PotionHooks {
    SPIRE_STATE(CunningPotion, int potency;)
    static SPIRE_FN constexpr PotionData data() { return {3, CardTarget::Self, true}; }
    static SPIRE_FN void constructPotion(const Game& g, PotionState& state, PotionId) {
        state.CunningPotion.potency = data().potency * (has_relic(g, RelicId::SacredBark) ? 2 : 1);
    }
    static SPIRE_FN int potency(const Game& g, int slot) { return g.run.potion_state[slot].CunningPotion.potency; }
    static SPIRE_FN void use(Game& g, int slot, int) {
        Card shiv = make_card(CardId::Shiv, 1);
        if (g.room_in_combat) {
            Effect e{};
            e.kind = EffectKind::MakeTempCardInHand;
            e.card = create_card(g, shiv);
            e.amount = potency(g, slot);
            add_bottom(g, e);
        }
    }
};
}
