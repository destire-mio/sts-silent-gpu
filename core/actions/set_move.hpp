#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SetMove : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        monster_setMove(g, e.target, e.amount, static_cast<Intent>(e.extra), e.card, 1);
    }
};
}
