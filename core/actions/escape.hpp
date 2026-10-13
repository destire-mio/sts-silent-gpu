#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Escape : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "TEXT"}; }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target; }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        monster_escape(g, e.target);
        if (monsters_basically_dead(g)) g.battle_over = true;
    }
};
}
