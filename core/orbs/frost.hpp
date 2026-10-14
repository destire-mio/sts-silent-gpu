#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"
SPIRE_RULE("rule:orbs.Frost")

namespace spire::units::orbs {
struct Frost {
    static SPIRE_FN constexpr int passive() { return 2; }
    static SPIRE_FN constexpr int evoke() { return 5; }
    static SPIRE_FN void onEvoke(Game& g, int orb) {
        Effect e{}; e.amount = e.info_base = g.orbs.objects[orb].evoke; e.damage_type = DamageType::Thorns;
        e.kind = EffectKind::Block;
        add_top(g, e);
    }
    static SPIRE_FN void onEndOfTurn(Game& g, int orb) {
        Effect e{}; e.amount = e.info_base = g.orbs.objects[orb].passive; e.damage_type = DamageType::Thorns;
        e.kind = EffectKind::Block; add_bottom(g, e);
    }
};
}
