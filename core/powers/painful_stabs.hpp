#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct PainfulStabs : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = -1; }
    static SPIRE_FN void onInflictDamage(Game& g, Power&, int, const DamageInfo& info, int amount, int) {
        if (amount > 0 && info.damage_type != DamageType::Thorns) {
            Effect e{}; e.kind = EffectKind::AddCardDiscard; e.card = g.cards.size; e.amount = 1;
            g.cards.push_back(make_card(CardId::Wound)); g.cards[e.card].uuid = g.next_card_uuid++;
            add_bottom(g, e);
        }
    }
};
}
