#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Barricade : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { PowerData data{}; data.keep_player_block = true; data.keep_monster_block = true; return data; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = -1; }
};
}
