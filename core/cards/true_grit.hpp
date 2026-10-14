#pragma once
#include "../hooks.hpp"
#include "../actions/exhaust.hpp"
namespace spire::units::cards {
struct TrueGrit : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 7, -1, 0, 2, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        add_bottom(g, g.cards[card].upgrades > 0 ? actions::Exhaust::make(1, false) : actions::Exhaust::make(1, true, false, false));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
