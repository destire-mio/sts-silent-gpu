#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SkipEnemiesTurn : ActionHooks {
    static SPIRE_FN void execute(Game& g, Effect &) { g.skip_monster_turn = true; }
};
}
