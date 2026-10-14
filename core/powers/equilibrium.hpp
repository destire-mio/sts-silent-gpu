#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Equilibrium : PowerHooks {
    static SPIRE_FN constexpr PowerData data() {
        PowerData d{};
        d.turn_based = true;
        return d;
    }
    static SPIRE_FN void atEndOfTurn(Game& g, Power &, int, bool is_player) {
        if (is_player)
            for (int i = 0; i < g.hand.size; ++i)
                if (!g.cards[g.hand[i]].ethereal)
                    g.cards[g.hand[i]].retain = true;
    }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (p.amount == 0) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = e.target = owner;
            e.power = PowerId::Equilibrium;
            add_bottom(g, e);
        } else {
            Effect e{};
            e.kind = EffectKind::ReducePower;
            e.source = e.target = owner;
            e.power = PowerId::Equilibrium;
            e.amount = 1;
            add_bottom(g, e);
        }
    }
};
}
