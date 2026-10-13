#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ObtainPotion : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("ObtainPotionAction.potion", potion_template, effect.state.ObtainPotion.potion, queue, index)
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(ObtainPotion, PotionId potion;)
    SPIRE_FN static Effect from_potion(PotionId potion, const PotionState& state) {
        Effect e{};
        e.kind = EffectKind::ObtainPotion;
        action_initialize_state(e);
        e.state.ObtainPotion.potion = potion;
        e.prepared_potion_state = state;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        for (int i = 0; i < 2; ++i) {
            if (g.run.potions[i] != PotionId::Empty) continue;
            g.run.potions[i] = e.state.ObtainPotion.potion;
            g.run.potion_state[i] = e.prepared_potion_state;
            return;
        }
    }
};
}
