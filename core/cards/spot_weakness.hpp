#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct SpotWeakness : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::SelfAndEnemy, 1, -1, -1, 3, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        Effect e{};
        e.kind = EffectKind::SpotWeakness;
        e.source = no_target;
        e.target = target;
        e.extra = g.cards[card].magic;
        add_bottom(g, e);
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
