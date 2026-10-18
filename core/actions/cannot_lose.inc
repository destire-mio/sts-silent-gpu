#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct CannotLose : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, Effect&) { g.cannot_lose = true; }
};
}
