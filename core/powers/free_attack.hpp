#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct FreeAttack : PowerHooks {
    SPIRE_WAITING_RULE("FreeAttackPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() {
        PowerData d{};
        d.free_attacks = true;
        return d;
    }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int owner, int card, Effect &) {
        if (card_data(g.cards[card].id).type == CardType::Attack && !g.cards[card].purge_on_use && p.amount > 0)
            if (--p.amount == 0) {
                Effect e{};
                e.kind = EffectKind::RemovePower;
                e.source = e.target = owner;
                e.power = PowerId::FreeAttack;
                add_top(g, e);
            }
    }
};
}
