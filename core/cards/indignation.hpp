#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Indignation : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, 3, 0, 0, 2, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::Indignation;
        e.source = e.target = no_target;
        e.amount = g.cards[card].magic;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 2;
            card.magic = card.base_magic;
        }
    }
};
}
