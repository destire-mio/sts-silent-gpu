#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Artifact : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { PowerData data{5, false, false}; data.blocks_debuffs = true; return data; }
    static SPIRE_FN void onSpecificTrigger(Game& g, Power& p, int owner) {
        Effect e{};
        e.kind = p.amount <= 0 ? EffectKind::RemovePower : EffectKind::ReducePower;
        e.target = owner;
        e.source = owner;
        e.power = PowerId::Artifact;
        e.amount = 1;
        add_top(g, e);
    }
};
}
