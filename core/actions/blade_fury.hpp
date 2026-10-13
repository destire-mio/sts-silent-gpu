#pragma once
#include "../hooks.hpp"
#include "../cards/shiv.hpp"

namespace spire::units::actions {
struct BladeFuryAction : ActionHooks {
    SPIRE_STATE(BladeFuryAction, bool upgrade;)
    SPIRE_FN static void prepare(Game&, Effect& e) {
        action_initialize_state(e);
        e.payload.state.BladeFuryAction.upgrade = e.flag;
        e.source = e.target = no_target;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        Card shiv = cards::Shiv::make(g);
        if (e.payload.state.BladeFuryAction.upgrade) cards::Shiv::upgrade(shiv);
        Effect copies{};
        copies.kind = EffectKind::MakeTempCardInHand;
        copies.card = create_card(g, shiv);
        copies.amount = g.hand.size;
        add_top(g, copies);
        Effect discard{};
        discard.kind = EffectKind::Discard;
        discard.source = discard.target = player_target;
        discard.amount = g.hand.size;
        add_top(g, discard);
        e.done = true;
    }
};
}
