#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Regen : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool) {
        Effect e{};
        e.kind = EffectKind::Regen;
        e.target = owner;
        e.amount = p.amount;
        add_top(g, e);
    }
};
}
