#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Discard : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        if (monsters_basically_dead(g)) return;
        if (g.hand.size <= e.amount) {
            while (!g.hand.empty()) discard_card(g, g.hand.size - 1, e.flag);
        } else {
            SPIRE_ASSERT(!e.flag);
            g.discard_remaining = e.amount;
            g.screen = Screen::Discard;
        }
    }
};
}
