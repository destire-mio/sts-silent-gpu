#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct LessonLearned : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 2, 10, -1, -1, 3, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        Effect e{};
        e.kind = EffectKind::LessonLearned;
        e.target = target;
        e.amount = e.info_base = g.cards[card].damage;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 3;
        }
    }
};
}
