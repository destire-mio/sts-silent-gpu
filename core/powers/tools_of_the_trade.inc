#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct ToolsOfTheTrade : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {25}; }
    static SPIRE_FN void atStartOfTurnPostDraw(Game& g, Power& p, int owner) {
        Effect draw{};
        draw.kind = EffectKind::Draw;
        draw.source = owner;
        draw.amount = p.amount;
        add_bottom(g, draw);
        Effect discard{};
        discard.kind = EffectKind::Discard;
        discard.source = owner;
        discard.target = owner;
        discard.amount = p.amount;
        add_bottom(g, discard);
    }
};
}
