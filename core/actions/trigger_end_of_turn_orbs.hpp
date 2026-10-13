#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct TriggerEndOfTurnOrbs : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_FN static void execute(Game&, const Effect&) {}
};
}
