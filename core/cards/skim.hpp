#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct Skim : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, 3, 0, 0, 1, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, units::actions::Draw::construct(g.cards[card].magic, true, -1, player_target));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
