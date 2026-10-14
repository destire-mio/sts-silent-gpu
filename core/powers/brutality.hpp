#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::powers {
struct Brutality : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurnPostDraw(Game& g, Power& p, int owner) {
        add_bottom(g, units::actions::Draw::construct(p.amount, true, -1, owner));
        Effect e{};
        e.kind = EffectKind::LoseHP;
        e.source = e.target = owner;
        e.amount = p.amount;
        add_bottom(g, e);
    }
};
}
