#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Focus : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, false, true}; }
    static SPIRE_FN bool isDebuff(const Power& p, int) { return p.amount <= 0; }
    static SPIRE_FN void stackPower(Game& g, Power& p, int owner, int amount) {
        p.amount += amount;
        if (p.amount == 0) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::Focus;
            add_top(g, e);
        }
        p.amount = max(-999, min(999, p.amount));
    }
    static SPIRE_FN void reducePower(Game& g, Power& p, int owner, int amount) {
        p.amount -= amount;
        if (p.amount == 0) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::Focus;
            add_top(g, e);
        }
        p.amount = max(-999, min(999, p.amount));
    }
};
}
