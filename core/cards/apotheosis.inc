#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Apotheosis : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 2, -1, -1, -1, 0, 0, 0, true, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        Effect e{};
        e.kind = EffectKind::Apotheosis;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 1);
        }
    }
};
}
