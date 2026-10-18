#pragma once
#include "core/hooks.hpp"
#include "core/actions/gain_energy.hpp"
SPIRE_RULE("rule:orbs.Plasma")

namespace spire::units::orbs {
struct Plasma {
    static SPIRE_FN constexpr int passive() { return 1; }
    static SPIRE_FN constexpr int evoke() { return 2; }
    static SPIRE_FN void onEvoke(Game& g, int orb) {
        Effect e{}; e.amount = e.info_base = g.orbs.objects[orb].evoke; e.damage_type = DamageType::Thorns;
        e = units::actions::GainEnergy::make(g.orbs.objects[orb].evoke);
        add_top(g, e);
    }
    static SPIRE_FN void onStartOfTurn(Game& g, int orb) {
        add_bottom(g, units::actions::GainEnergy::make(g.orbs.objects[orb].passive));
    }
};
}
