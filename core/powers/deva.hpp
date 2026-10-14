#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Deva : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    SPIRE_STATE(Deva, int energyGainAmount;)
    static SPIRE_FN void initialPower(Game &, Power& p, int) {
        p.amount = 1;
        p.state.Deva.energyGainAmount = 1;
    }
    static SPIRE_FN void stackPower(Game &, Power& p, int, int amount) {
        if (p.amount != -1)
            p.amount += amount;
        ++p.state.Deva.energyGainAmount;
    }
    static SPIRE_FN void onEnergyRecharge(Game& g, Power& p, int) {
        gain_energy(g, p.state.Deva.energyGainAmount);
        p.state.Deva.energyGainAmount += p.amount;
    }
};
}
