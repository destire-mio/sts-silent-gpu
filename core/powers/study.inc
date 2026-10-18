#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Study : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int, bool) {
        Effect e{};
        e.kind = EffectKind::MakeTempCardInDrawPile;
        action_initialize_state(e);
        e.card = create_card(g, make_card(CardId::Insight, 0));
        e.amount = p.amount;
        e.payload.state.MakeTempCardInDrawPile.random_spot = true;
        add_bottom(g, e);
    }
};
}
