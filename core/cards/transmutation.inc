#pragma once
#include "core/hooks.hpp"
#include "core/actions/transmutation.hpp"

namespace spire::units::cards {
struct Transmutation : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, -1, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Card& c = g.cards[card];
        if (c.energy_on_use < g.player.energy) c.energy_on_use = g.player.energy;
        add_bottom(g, actions::TransmutationAction::construct(player_target, c.upgrades > 0, c.free_to_play_once, c.energy_on_use));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
