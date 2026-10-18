#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct IncreaseMaxHp : ActionHooks {
    SPIRE_STATE(IncreaseMaxHp, float increasePercent;)
    SPIRE_FN static Effect construct(int target, float percent, bool show_effect) {
        (void)show_effect;
        Effect e{};
        e.kind = EffectKind::IncreaseMaxHp;
        action_initialize_state(e);
        e.target = target;
        e.source = no_target;
        e.payload.state.IncreaseMaxHp.increasePercent = percent;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        Creature& target = get_creature(g, e.target);
        const int amount = floor_int(static_cast<float>(target.max_hp) * e.payload.state.IncreaseMaxHp.increasePercent + 0.5f);
        target.max_hp += amount;
        heal_creature(g, e.target, amount);
    }
};
}
