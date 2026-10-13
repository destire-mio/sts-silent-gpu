#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct BlueCandle : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250, false, true}; }
    static SPIRE_FN void onUseCard(Game& g, int, int card, Effect& use) {
        if (card_def(g.cards[card].id).type == CardType::Curse) {
            Effect e{};
            e.kind = EffectKind::LoseHP;
            e.amount = 1;
            add_bottom(g, e);
            g.cards[card].exhaust = true;
            use.flag = true;
        }
    }
};
}
