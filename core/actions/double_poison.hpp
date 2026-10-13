#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DoublePoison : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "DEBUFF"}; }
    SPIRE_FN static Effect construct(int target, int source) {
        Effect e{};
        e.kind = EffectKind::DoublePoison;
        action_initialize_state(e);
        e.target = target;
        e.source = source;
        return e;
    }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.target == no_target) return;
        const Creature& target = get_creature(g, e.target);
        const int index = find_power(g, target, "Poison");
        if (index < 0) return;
        Effect apply{};
        apply.kind = EffectKind::ApplyPower;
        apply.source = e.source;
        apply.target = e.target;
        apply.power = target.powers.at(g.power_objects, index).id;
        apply.amount = target.powers.at(g.power_objects, index).amount;
        add_top(g, apply);
    }
};
}
