#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct TriplePoisonAction : ActionHooks {
    SPIRE_FN static void execute(Game& g, Effect& e) {
        const Creature& target = get_creature(g, e.target);
        const int poison = find_power(target, "Poison");
        if (poison >= 0) {
            Effect apply{};
            apply.kind = EffectKind::ApplyPower;
            apply.source = e.source;
            apply.target = e.target;
            apply.power = target.powers[poison].id;
            apply.amount = target.powers[poison].amount * 2;
            add_top(g, apply);
        }
        e.done = true;
    }
};
}
