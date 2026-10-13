#pragma once
#include "../hooks.hpp"
#include "../actions/conditional_draw.hpp"

namespace spire::units::cards {
struct Impatience : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 0, -1, -1, 2, 0, 0, 1, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, units::actions::ConditionalDrawAction::construct(g.cards[card].magic, CardType::Attack));
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
