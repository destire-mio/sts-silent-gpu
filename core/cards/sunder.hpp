#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Sunder : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 3, 24, -1, -1, 8, 0, 0, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        if (target != no_target) {
            Effect v{};
            v.kind = EffectKind::VFX;
            v.source = v.target = no_target;
            add_bottom(g, v);
        }
        Effect e{};
        e.kind = EffectKind::Sunder;
        e.target = target;
        e.amount = e.info_base = g.cards[card].damage;
        e.extra = 3;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 8;
        }
    }
};
}
