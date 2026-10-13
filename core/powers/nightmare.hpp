#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Nightmare : PowerHooks {
    SPIRE_STATE(Nightmare, int card;)
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power&, int) { SPIRE_ASSERT(false); }
    static SPIRE_FN Power construct(Game& g, int amount, int copy_me) {
        Power p{PowerId::Nightmare, amount};
        power_initialize_state(p);
        const int card = g.cards.size;
        g.cards.push_back(card_makeStatEquivalentCopy(g, copy_me));
        g.cards[card].uuid = card + 1;
        card_resetAttributes(g, card);
        p.state.Nightmare.card = card;
        return p;
    }
    static SPIRE_FN void atStartOfTurn(Game& g, Power&, int owner) {
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Nightmare;
        add_bottom(g, e);
    }
};
}
