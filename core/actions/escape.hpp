#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Escape : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target; }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        monster_escape(g, e.target);
        if (monsters_basically_dead(g)) g.battle_over = true;
    }
};
}
