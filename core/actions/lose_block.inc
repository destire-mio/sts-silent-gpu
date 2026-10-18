#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct LoseBlock : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    SPIRE_FN static Effect construct(int target, int source, int amount) {
        Effect e{};
        e.kind = EffectKind::LoseBlock;
        action_initialize_state(e);
        e.target = target;
        e.source = source;
        e.amount = amount;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        Creature& target = get_creature(g, e.target);
        if (target.block == 0) return;
        target.block = max(0, target.block - e.amount);
    }
};
}
