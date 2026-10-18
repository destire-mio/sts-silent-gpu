#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Nirvana : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onScry(Game& g, Power& p, int owner) {
        Effect e{};
        e.kind = EffectKind::Block;
        e.source = no_target;
        e.target = owner;
        e.amount = p.amount;
        add_bottom(g, e);
    }
};
}
