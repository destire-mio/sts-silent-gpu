#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct NoDraw : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true, false, false, true, false, false}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = -1; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power&, int owner, bool is_player) {
        if (!is_player) return;
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.target = owner;
        e.power = PowerId::NoDraw;
        add_bottom(g, e);
    }
};
}
