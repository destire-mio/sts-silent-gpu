#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Heal : ActionHooks {
    SPIRE_FN static constexpr ActionData data() { return {true, "HEAL"}; }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (e.target == player_target) heal_creature(g, e.target, e.amount);
        else monster_heal(g, e.target, e.amount);
    }
};
}
