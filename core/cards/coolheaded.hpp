#pragma once
#include "../hooks.hpp"
#include "../actions/channel.hpp"
#include "../actions/draw.hpp"
namespace spire::units::cards {
struct Coolheaded : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, -1, 1, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::Channel::make(make_orb(g, OrbKind::Frost)));
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
