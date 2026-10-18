#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct MedicalKit : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150, true}; }
    static SPIRE_FN void onUseCard(Game& g, int, int card, Effect& use) {
        if (card_data(g.cards[card].id).type == CardType::Status) {
            g.cards[card].exhaust = true;
            use.flag = true;
        }
    }
};
}
