#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct GainStrength : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true, false}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = max(-999, min(999, p.amount)); }
    static SPIRE_FN void stackPower(Game& g, Power& p, int owner, int amount) {
        p.amount += amount;
        if (p.amount == 0) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::GainStrength;
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
            e.power = PowerId::GainStrength;
            add_top(g, e);
        }
        p.amount = max(-999, min(999, p.amount));
    }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool) {
        Effect strength{};
        strength.kind = EffectKind::ApplyPower;
        strength.source = owner;
        strength.target = owner;
        strength.power = PowerId::Strength;
        strength.amount = p.amount;
        add_bottom(g, strength);
        Effect remove{};
        remove.kind = EffectKind::RemovePower;
        remove.source = owner;
        remove.target = owner;
        remove.power = PowerId::GainStrength;
        add_bottom(g, remove);
    }
};
}
