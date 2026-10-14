#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::powers {
struct DarkEmbrace : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onExhaust(Game& g, Power& p, int owner, int) {
        if (!monsters_basically_dead(g)) {
            add_bottom(g, units::actions::Draw::construct(p.amount, true, -1, owner));
        }
    }
};
}
