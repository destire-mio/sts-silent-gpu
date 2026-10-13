#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ShowCard : ActionHooks {
    SPIRE_FN static Effect construct(int card) {
        Effect e{};
        e.kind = EffectKind::ShowCard;
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
        erase_card(g.limbo, e.card);
        g.player.card_in_use = -1;
    }
};
}
