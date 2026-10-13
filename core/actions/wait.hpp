#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Wait : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) {
        e.source = no_target;
        e.target = no_target;
        e.start_duration = 0.0f;
    }
    SPIRE_FN static void execute(Game&, Effect& e) { finish_action_duration(e); }
};
}
