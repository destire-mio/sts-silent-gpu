#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct PenNib : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { PowerData data{6}; data.turn_based = true; return data; }
    static SPIRE_FN void onUseCard(Game& g, Power&, int owner, int card, Effect&) {
        if (card_data(g.cards[card].id).type == CardType::Attack) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::PenNib;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN float atDamageGive(const Game&, const Power&, int, float value, DamageType type, int = -1) {
        return type == DamageType::Normal ? value * 2.0f : value;
    }
};
}
