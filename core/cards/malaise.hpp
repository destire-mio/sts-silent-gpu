#pragma once
#include "../hooks.hpp"
#include "../actions/malaise.hpp"

namespace spire::units::cards {
struct Malaise : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Enemy, -1, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        const Card& c = g.cards[card];
        add_bottom(g, actions::MalaiseAction::construct(player_target, target, c.upgrades > 0, c.free_to_play_once, c.energy_on_use));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
