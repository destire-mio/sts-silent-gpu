#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Rage : PowerHooks {
    SPIRE_WAITING_RULE("RagePower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int, int card, Effect &) {
        if (card_data(g.cards[card].id).type == CardType::Attack)
            queue_block(g, player_target, p.amount);
    }
    static SPIRE_FN void atEndOfTurn(Game& g, Power &, int owner, bool) {
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = e.target = owner;
        e.power = PowerId::Rage;
        add_bottom(g, e);
    }
};
}
