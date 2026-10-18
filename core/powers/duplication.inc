#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Duplication : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int owner, int card, Effect& use) {
        if (!g.cards[card].purge_on_use && p.amount > 0) {
            queue_card_copy(g, card, use.target);
            --p.amount;
            if (p.amount == 0) {
                Effect e{};
                e.kind = EffectKind::RemovePower;
                e.source = owner;
                e.target = owner;
                e.power = PowerId::Duplication;
                add_bottom(g, e);
            }
        }
    }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        Effect e{};
        e.kind = p.amount == 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Duplication;
        e.amount = 1;
        add_bottom(g, e);
    }
};
}
