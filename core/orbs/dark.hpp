#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"
SPIRE_RULE("rule:orbs.Dark")

namespace spire::units::orbs {
struct Dark {
    static SPIRE_FN constexpr int passive() { return 6; }
    static SPIRE_FN constexpr int evoke() { return 6; }
    static SPIRE_FN void onEvoke(Game& g, int orb) {
        Effect e{}; e.amount = e.info_base = g.orbs.objects[orb].evoke; e.damage_type = DamageType::Thorns;
        e.kind = EffectKind::DarkOrbEvoke;
        add_top(g, e);
    }
    static SPIRE_FN void onEndOfTurn(Game& g, int orb) {
        g.orbs.objects[orb].evoke += g.orbs.objects[orb].passive; apply_orb_focus(g, orb);
    }
};
}
