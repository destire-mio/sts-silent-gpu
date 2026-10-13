#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SetDontTrigger : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        g.cards[e.card].dont_trigger_on_use = e.flag;
    }
};
}
