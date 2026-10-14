#pragma once
#include "../hooks.hpp"
#include "../actions/change_stance.hpp"

namespace spire::units::powers {
struct Mantra : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    SPIRE_SNAPSHOT("MantraPower.PRAYER_REQUIRED", scalar, 10)
    static SPIRE_FN void initialPower(Game& g, Power& p, int) { g.manager.mantra_gained += p.amount; }
    static SPIRE_FN void stackPower(Game& g, Power& p, int owner, int amount) {
        if (p.amount != -1)
            p.amount += amount;
        if (p.amount >= 10) {
            add_top(g, units::actions::ChangeStance::make(Stance::Divinity));
            p.amount -= 10;
            if (p.amount <= 0) {
                Effect e{};
                e.kind = EffectKind::RemovePower;
                e.source = e.target = owner;
                e.power = PowerId::Mantra;
                add_top(g, e);
            }
        }
    }
};
}
