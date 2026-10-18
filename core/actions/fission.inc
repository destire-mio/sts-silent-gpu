#pragma once
#include "core/hooks.hpp"
#include "core/actions/draw.hpp"
#include "core/actions/gain_energy.hpp"

namespace spire::units::actions {
struct Fission : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "ENERGY"}; }
    SPIRE_SNAPSHOT("FissionAction.upgraded", scalar, effect.flag)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        const int count = filled_orb_count(g);
        add_top(g, units::actions::Draw::construct(count));
        add_top(g, units::actions::GainEnergy::make(count));
        Effect a{};
        a.kind = e.flag ? EffectKind::EvokeAllOrbs : EffectKind::RemoveAllOrbs;
        a.source = a.target = no_target;
        add_top(g, a);
    }
};
}
