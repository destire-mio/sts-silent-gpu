#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Rebound : PowerHooks {
    static SPIRE_FN constexpr PowerData data() {
        PowerData d{};
        d.turn_based = true;
        return d;
    }
    SPIRE_STATE(Rebound, bool justEvoked;)
    static SPIRE_FN void initialPower(Game &, Power& p, int) {
        p.amount = 1;
        p.state.Rebound.justEvoked = true;
    }
    static SPIRE_FN void onAfterUseCard(Game& g, Power& p, int owner, int card, Effect& use) {
        if (p.state.Rebound.justEvoked) {
            p.state.Rebound.justEvoked = false;
            return;
        }
        if (card_data(g.cards[card].id).type != CardType::Power)
            use.payload.state.UseCard.rebound_card = true;
        Effect e{};
        e.kind = EffectKind::ReducePower;
        e.source = e.target = owner;
        e.power = PowerId::Rebound;
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN void atEndOfTurn(Game& g, Power &, int owner, bool is_player) {
        if (is_player) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = e.target = owner;
            e.power = PowerId::Rebound;
            add_bottom(g, e);
        }
    }
};
}
