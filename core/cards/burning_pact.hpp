#pragma once
#include "../hooks.hpp"
#include "../actions/exhaust.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct BurningPact : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, 2, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::Exhaust::make(1, false, false));
        add_bottom(g, actions::Draw::construct(g.cards[card].magic, true, -1, player_target));
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
