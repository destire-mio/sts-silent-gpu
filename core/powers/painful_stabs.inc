#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct PainfulStabs : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = -1; }
    static SPIRE_FN void onInflictDamage(Game& g, Power&, int, const DamageInfo& info, int amount, int) {
        if (amount > 0 && info.damage_type != DamageType::Thorns) {
            Effect e{};
            e.kind = EffectKind::AddCardDiscard;
            e.card = create_card(g, make_card(CardId::Wound));
            e.amount = 1;
            add_bottom(g, e);
        }
    }
};
}
