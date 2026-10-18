#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Invincible : PowerHooks {
    SPIRE_STATE(Invincible, int maxAmt;)
    static SPIRE_FN constexpr PowerData data() { return {99}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) {
        p.state.Invincible.maxAmt = p.amount;
    }
    static SPIRE_FN int onAttackedToChangeDamage(Game&, Power& p, int, const DamageInfo&, int amount) {
        if (amount > p.amount) amount = p.amount;
        p.amount -= amount;
        if (p.amount < 0) p.amount = 0;
        return amount;
    }
    static SPIRE_FN void atStartOfTurn(Game&, Power& p, int) {
        p.amount = p.state.Invincible.maxAmt;
    }
};
}
