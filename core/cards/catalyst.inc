#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Catalyst : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Enemy, 1, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        Effect e{};
        e.kind = g.cards[card].upgrades == 0 ? EffectKind::DoublePoison : EffectKind::TriplePoisonAction;
        e.source = player_target;
        e.target = target;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
