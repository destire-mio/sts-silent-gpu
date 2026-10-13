#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct AfterImage : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int, int, Effect&) {
        queue_block(g, player_target, p.amount);
    }
};
}
