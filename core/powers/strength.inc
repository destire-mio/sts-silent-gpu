#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Strength : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, false, true}; }
    static SPIRE_FN bool isDebuff(const Power& p, int) { return p.amount <= 0; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = max(-999, min(999, p.amount)); }
    static SPIRE_FN void stackPower(Game& g, Power& p, int owner, int amount) {
        p.amount += amount;
        if (p.amount == 0) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.target = owner;
            e.power = PowerId::Strength;
            add_top(g, e);
        }
        p.amount = max(-999, min(999, p.amount));
    }
    static SPIRE_FN void reducePower(Game& g, Power& p, int owner, int amount) {
        p.amount -= amount;
        if (p.amount == 0) {
            Effect e{};
            e.kind = EffectKind::RemovePower;
            e.target = owner;
            e.power = PowerId::Strength;
            add_top(g, e);
        }
        p.amount = max(-999, min(999, p.amount));
    }
    static SPIRE_FN float atDamageGive(const Game&, const Power& p, int, float value, DamageType type, int = -1) {
        return type == DamageType::Normal ? value + static_cast<float>(p.amount) : value;
    }
};
}
