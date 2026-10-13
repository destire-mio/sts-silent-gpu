#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ShowCardAndPoof : ActionHooks {
    SPIRE_SNAPSHOT("ShowCardAndPoofAction.card", card, effect.card)
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_FN static Effect construct(int card) {
        Effect e{};
        e.kind = EffectKind::ShowCardAndPoof;
        action_initialize_state(e);
        e.card = card;
        return e;
    }
    SPIRE_FN static void prepare(Game&, Effect& e) {
        e.source = no_target;
        e.target = player_target;
        e.amount = 1;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        remove_from_pile(g.limbo, e.card);
        g.player.card_in_use = -1;
    }
};
}
