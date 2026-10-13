#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Shifting : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = -1; }
    static SPIRE_FN int onAttacked(Game& g, Power&, int owner, const DamageInfo&, int amount) {
        if (amount > 0) {
            Effect strength{};
            strength.kind = EffectKind::ApplyPower;
            strength.source = owner;
            strength.target = owner;
            strength.power = PowerId::Strength;
            strength.amount = -amount;
            add_top(g, strength);
            if (!has_power(get_creature(g, owner), PowerId::Artifact)) {
                Effect gain{};
                gain.kind = EffectKind::ApplyPower;
                gain.source = owner;
                gain.target = owner;
                gain.power = PowerId::GainStrength;
                gain.amount = amount;
                add_top(g, gain);
            }
        }
        return amount;
    }
};
}
