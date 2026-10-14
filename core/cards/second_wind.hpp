#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct SecondWind : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 5, -1, 0, 2, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::BlockPerNonAttack;
        e.extra = g.cards[card].block;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
