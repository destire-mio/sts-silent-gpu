#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct WristBlade : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 999}; }
    static SPIRE_FN float atDamageModify(const Game& g, int, float value, int card) {
        const Card& c = get_card(g, card);
        return c.cost_for_turn == 0 || (c.free_to_play_once && c.cost != -1) ? value + 4.0f : value;
    }
};
}
