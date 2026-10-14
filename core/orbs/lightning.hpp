#pragma once
#include "../hooks.hpp"
#include "../actions/gain_energy.hpp"
SPIRE_RULE("rule:orbs.Lightning")

namespace spire::units::orbs {
struct Lightning {
    static SPIRE_FN constexpr int passive() { return 3; }
    static SPIRE_FN constexpr int evoke() { return 8; }
    static SPIRE_FN void onEvoke(Game& g, int orb) {
        Effect e{}; e.amount = e.info_base = g.orbs.objects[orb].evoke; e.damage_type = DamageType::Thorns;
        e.kind = EffectKind::LightningOrbEvoke; e.flag = has_power(g, g.player.creature, "Electro");
        add_top(g, e);
    }
    static SPIRE_FN void onEndOfTurn(Game& g, int orb) {
        Effect e{}; e.amount = e.info_base = g.orbs.objects[orb].passive; e.damage_type = DamageType::Thorns;
        e.flag = has_power(g, g.player.creature, "Electro");
        e.kind = e.flag ? EffectKind::LightningOrbEvoke : EffectKind::LightningOrbPassive; e.extra = orb; add_bottom(g, e);
    }
};
}
