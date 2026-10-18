#pragma once
#include "core/hooks.hpp"

namespace spire::units::relics {
struct Orrery : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 150}; }
    static SPIRE_FN void onEquip(Game& g, int) {
        for (int i = 0; i < 4; ++i) add_room_card_reward(g);
        open_room_card_rewards(g);
    }
};
}
