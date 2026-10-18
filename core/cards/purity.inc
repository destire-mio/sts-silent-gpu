#pragma once
#include "core/hooks.hpp"
#include "core/actions/exhaust.hpp"

namespace spire::units::cards {
struct Purity : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 0, -1, -1, 3, 0, 0, 2, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::Exhaust::make(g.cards[card].magic, false, true, true));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
