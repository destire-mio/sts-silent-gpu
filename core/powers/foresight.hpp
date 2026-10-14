#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Foresight : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int) {
        if (g.draw.empty()) {
            Effect e{};
            e.kind = EffectKind::Shuffle;
            add_top(g, e);
        }
        Effect e{};
        e.kind = EffectKind::Scry;
        e.amount = p.amount;
        add_bottom(g, e);
    }
};
}
