#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct RollMove : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("RollMoveAction.monster", monster, effect.target)
    SPIRE_FN static void execute(Game& g, const Effect& e) { roll_monster_move(g, e.target); }
};
}
