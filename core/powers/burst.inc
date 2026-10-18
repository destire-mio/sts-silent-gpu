#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Burst : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int owner, int card, Effect& use) {
        if (!g.cards[card].purge_on_use && card_data(g.cards[card].id).type == CardType::Skill && p.amount > 0) {
            queue_card_copy(g, card, use.target);
            --p.amount;
            if (p.amount == 0) {
                Effect e{};
                e.kind = EffectKind::RemovePower;
                e.source = owner;
                e.target = owner;
                e.power = PowerId::Burst;
                add_top(g, e);
            }
        }
    }
    static SPIRE_FN void atEndOfTurn(Game& g, Power&, int owner, bool is_player) {
        if (!is_player) return;
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Burst;
        add_bottom(g, e);
    }
};
}
