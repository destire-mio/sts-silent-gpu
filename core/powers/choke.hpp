#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Choke : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power&, int owner) {
        Effect e{};
        e.kind = EffectKind::RemovePower;
        e.source = owner;
        e.target = owner;
        e.power = PowerId::Choke;
        add_bottom(g, e);
    }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int owner, int, Effect&) {
        Effect e{};
        e.kind = EffectKind::LoseHP;
        e.source = no_target;
        e.target = owner;
        e.amount = p.amount;
        add_bottom(g, e);
    }
};
}
