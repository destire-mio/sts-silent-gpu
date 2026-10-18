#pragma once
#include "core/hooks.hpp"
#include "core/actions/expertise.hpp"

namespace spire::units::cards {
struct Expertise : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, -1, 6, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::Expertise::construct(player_target, g.cards[card].magic));
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
