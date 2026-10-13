#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ObtainPotion : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("ObtainPotionAction.potion", potion_template, effect.payload.potion.id, queue, index)
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_FN static Effect from_potion(PotionId potion, const PotionState& state) {
        Effect e{};
        e.kind = EffectKind::ObtainPotion;
        action_initialize_state(e);
        e.payload = EffectPayload(PreparedPotion{state, potion});
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        for (int i = 0; i < g.run.relics.size; ++i)
            if (relic_data(g.run.relics[i].id).blocks_potions) return;
        obtain_potion(g, e.payload.potion.id, e.payload.potion.state);
    }
};
}
