#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct FeelNoPain : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onExhaust(Game& g, Power& p, int owner, int) {
        Effect e{};
        e.kind = EffectKind::Block;
        e.source = no_target;
        e.target = owner;
        e.amount = p.amount;
        add_bottom(g, e);
    }
};
}
