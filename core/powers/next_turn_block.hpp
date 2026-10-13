#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct NextTurnBlock : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int owner) {
        queue_block(g, owner, p.amount);
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::NextTurnBlock;
        add_bottom(g, e);
    }
};
}
