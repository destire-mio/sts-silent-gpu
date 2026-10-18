#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct DrawCardNextTurn : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {20}; }
    static SPIRE_FN void atStartOfTurnPostDraw(Game& g, Power& p, int owner) {
        Effect draw{};
        draw.kind = EffectKind::Draw;
        draw.source = owner;
        draw.amount = p.amount;
        add_bottom(g, draw);
        Effect remove{};
        remove.kind = EffectKind::RemovePower;
        remove.source = owner;
        remove.target = owner;
        remove.power = PowerId::DrawCardNextTurn;
        add_bottom(g, remove);
    }
};
}
