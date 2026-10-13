#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Ritual : PowerHooks {
    SPIRE_SNAPSHOT("RitualPower.skipFirst", scalar, power.skip_first)
    SPIRE_SNAPSHOT("RitualPower.onPlayer", scalar, owner == player_target)
    static SPIRE_FN constexpr PowerData data() { return {5, false, false}; }
    static SPIRE_FN void atEndOfTurn(Game& g, Power& p, int owner, bool is_player) {
        if (is_player) queue_power(g, owner, owner, PowerId::Strength, p.amount);
    }
    static SPIRE_FN void atEndOfRound(Game& g, Power& p, int owner) {
        if (owner == player_target) return;
        if (p.skip_first) p.skip_first = false;
        else queue_power(g, owner, owner, PowerId::Strength, p.amount);
    }
};
}
