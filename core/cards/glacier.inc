#pragma once
#include "core/hooks.hpp"
#include "core/actions/channel.hpp"

namespace spire::units::cards {
struct Glacier : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 2, -1, 7, 2, 0, 3, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        for (int i = 0; i < g.cards[card].magic; ++i)
            add_bottom(g, units::actions::Channel::make(make_orb(g, OrbKind::Frost)));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
