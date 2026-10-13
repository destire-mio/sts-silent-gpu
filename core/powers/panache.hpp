#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Panache : PowerHooks {
    SPIRE_STATE(Panache, int damage;)
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) {
        p.state.Panache.damage = p.amount;
        p.amount = 5;
    }
    static SPIRE_FN void stackPower(Game&, Power& p, int, int amount) {
        p.state.Panache.damage += amount;
    }
    static SPIRE_FN void onUseCard(Game&, Power& p, int, int, Effect&) {
        --p.amount;
        if (p.amount == 0) p.amount = 5;
    }
    static SPIRE_FN void atStartOfTurn(Game&, Power& p, int) { p.amount = 5; }
};
}
