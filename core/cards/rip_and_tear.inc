#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct RipAndTear : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::AllEnemies, 1, 7, -1, 2, 2, 0, 0, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        for (int i = 0; i < g.cards[card].magic; ++i) {
            Effect e{};
            e.kind = EffectKind::NewRipAndTear;
            e.source = e.target = no_target;
            e.card = card;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 2;
        }
    }
};
}
