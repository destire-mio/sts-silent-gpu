#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Stasis : PowerHooks {
    SPIRE_SNAPSHOT("StasisPower.card", card, power.state.Stasis.card)
    SPIRE_STATE(Stasis, int card;)
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void constructPower(Game&, Power& p, int, int, int, int card) {
        power_initialize_state(p);
        p.state.Stasis.card = card;
        p.amount = -1;
    }
    static SPIRE_FN void onDeath(Game& g, Power& p, int) {
        if (g.hand.size == 10) {
            Effect e{};
            e.kind = EffectKind::AddCardDiscard;
            e.card = p.state.Stasis.card;
            e.amount = 1;
            e.flag = true;
            add_bottom(g, e);
        } else {
            Effect e{};
            e.kind = EffectKind::MakeTempCardInHand;
            e.card = p.state.Stasis.card;
            e.amount = 1;
            e.state.MakeTempCardInHand.same_uuid = true;
            add_bottom(g, e);
        }
    }
};
}
