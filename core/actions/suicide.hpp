#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Suicide : ActionHooks {
    SPIRE_FN static constexpr ActionData data() { return {true}; }
    SPIRE_STATE(Suicide, bool suppress_relics;)
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        Creature& c = g.monsters[e.target].creature;
        c.gold = 0;
        c.hp = 0;
        monster_die(g, e.target, !e.state.Suicide.suppress_relics);
    }
};
}
