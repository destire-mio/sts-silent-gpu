#pragma once
#include "../hooks.hpp"
#include "../actions/change_stance.hpp"

namespace spire::units::powers {
struct WrathNextTurn : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game &, Power& p, int) { p.amount = -1; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int owner) {
        add_bottom(g, units::actions::ChangeStance::make(Stance::Wrath));
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = e.target = owner;
        e.power = PowerId::WrathNextTurn;
        e.extra = p.instance;
        add_bottom(g, e);
    }
};
}
