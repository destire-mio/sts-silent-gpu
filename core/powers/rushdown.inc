#pragma once
#include "core/hooks.hpp"
#include "core/actions/draw.hpp"

namespace spire::units::powers {
struct Rushdown : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onChangeStance(Game& g, Power& p, int owner, int old_stance, int new_stance) {
        if (old_stance != new_stance && new_stance == static_cast<int>(Stance::Wrath)) {
            add_bottom(g, units::actions::Draw::construct(p.amount, true, -1, owner));
        }
    }
};
}
