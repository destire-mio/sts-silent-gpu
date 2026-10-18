#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Buffer : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) { p.amount += amount; }
    static SPIRE_FN int onAttackedToChangeDamage(Game& g, Power&, int owner, const DamageInfo&, int amount) {
        if (amount > 0) {
            Effect e{};
            e.kind = EffectKind::ReducePower;
            e.source = owner;
            e.target = owner;
            e.power = PowerId::Buffer;
            e.amount = 1;
            add_top(g, e);
        }
        return 0;
    }
};
}
