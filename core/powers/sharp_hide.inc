#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct SharpHide : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int owner, int card, Effect&) {
        if (card_data(g.cards[card].id).type == CardType::Attack) {
            Effect e{};
            e.kind = EffectKind::Damage;
            e.source = owner;
            e.target = player_target;
            e.amount = e.info_base = p.amount;
            e.damage_type = DamageType::Thorns;
            add_bottom(g, e);
        }
    }
};
}
