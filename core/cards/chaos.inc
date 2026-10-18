#pragma once
#include "core/hooks.hpp"
#include "core/actions/channel.hpp"
namespace spire::units::cards {
struct Chaos : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, -1, 1, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        const OrbKind choices[] = {OrbKind::Dark, OrbKind::Frost, OrbKind::Lightning, OrbKind::Plasma};
        for (int i = 0; i < (g.cards[card].upgrades ? 2 : 1); ++i)
            add_bottom(g, actions::Channel::make(make_orb(g, choices[g.rng.card_random.random(3)])));
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
