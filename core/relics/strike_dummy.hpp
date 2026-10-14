#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct StrikeDummy : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN float atDamageModify(const Game& g, int, float value, int card) {
        return card_data(get_card(g, card).id).strike ? value + 3.0f : value;
    }
};
}
