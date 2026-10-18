#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Poison : PowerHooks {
    SPIRE_SNAPSHOT("PoisonPower.source", creature, power.state.Poison.source)
    SPIRE_STATE(Poison, int source;)
    static SPIRE_FN constexpr PowerData data() { PowerData data{5, true}; data.turn_based = true; return data; }
    static SPIRE_FN void constructPower(Game&, Power& p, int, int source, int, int) {
        power_initialize_state(p);
        p.state.Poison.source = source;
        p.amount = min(9999, p.amount);
    }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int owner) {
        if (!g.battle_over && !monsters_basically_dead(g)) {
            Effect e{};
            e.kind = EffectKind::PoisonLoseHp;
            e.source = p.state.Poison.source;
            e.target = owner;
            e.amount = p.amount;
            add_bottom(g, e);
        }
    }
};
}
