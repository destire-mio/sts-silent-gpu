#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Establishment : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {25}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int, bool) {
        Effect e{};
        e.kind = EffectKind::EstablishmentPower;
        e.extra = p.amount;
        e.source = e.target = no_target;
        add_bottom(g, e);
    }
};
}
