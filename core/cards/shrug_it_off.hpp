#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"
namespace spire::units::cards {
struct ShrugItOff : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 8, -1, 0, 3, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        add_bottom(g, actions::Draw::construct(1, true, -1, player_target));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
