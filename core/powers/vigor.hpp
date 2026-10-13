#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Vigor : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN float atDamageGive(const Game&, const Power& p, int, float value, DamageType type, int = -1) {
        return type == DamageType::Normal ? value + static_cast<float>(p.amount) : value;
    }
    static SPIRE_FN void onUseCard(Game& g, Power&, int owner, int card, Effect&) {
        if (card_def(g.cards[card].id).type == CardType::Attack) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::Vigor;
            add_bottom(g, e);
        }
    }
};
}
