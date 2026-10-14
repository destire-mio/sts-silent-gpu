#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::powers {
struct Rushdown : PowerHooks {
    SPIRE_WAITING_RULE("RushdownPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onChangeStance(Game& g, Power& p, int owner, int old_stance, int new_stance) {
        if (old_stance != new_stance && new_stance == static_cast<int>(Stance::Wrath)) {
            add_bottom(g, units::actions::Draw::construct(p.amount, true, -1, owner));
        }
    }
};
}
