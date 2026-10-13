#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Nightmare : PowerHooks {
    SPIRE_STATE(Nightmare, int card;)
    static SPIRE_FN constexpr PowerData data() { PowerData data{}; data.stacks_separately = true; return data; }
    static SPIRE_FN void constructPower(Game& g, Power& p, int, int, int, int copy_me) {
        power_initialize_state(p);
        const int card = g.cards.size;
        g.cards.push_back(card_makeStatEquivalentCopy(g, copy_me));
        g.cards[card].uuid = card + 1;
        card_resetAttributes(g, card);
        p.state.Nightmare.card = card;
    }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int owner) {
        Effect copy{};
        copy.kind = EffectKind::MakeTempCardInHand;
        copy.card = p.state.Nightmare.card;
        copy.amount = p.amount;
        add_bottom(g, copy);
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Nightmare;
        add_bottom(g, e);
    }
};
}
