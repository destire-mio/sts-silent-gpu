#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"

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
