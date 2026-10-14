#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Amplify : PowerHooks {
    SPIRE_WAITING_RULE("AmplifyPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int owner, int card, Effect& use) {
        if (!g.cards[card].purge_on_use && card_data(g.cards[card].id).type == CardType::Power && p.amount > 0) {
            queue_card_copy(g, card, use.target);
            if (--p.amount == 0) {
                Effect e{};
                e.kind = EffectKind::RemovePower;
                e.source = e.target = owner;
                e.power = PowerId::Amplify;
                add_bottom(g, e);
            }
        }
    }
    static SPIRE_FN void atEndOfTurn(Game& g, Power &, int owner, bool is_player) {
        if (is_player) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = e.target = owner;
            e.power = PowerId::Amplify;
            add_bottom(g, e);
        }
    }
};
}
