#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct ClearCardQueue : ActionHooks {
    SPIRE_FN static void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    SPIRE_FN static void execute(Game& g, const Effect&) {
        for (int i = 0; i < g.manager.card_queue.size; ++i)
            remove_from_pile(g.limbo, g.manager.card_queue[i].card);
        g.manager.card_queue.clear();
    }
};
}
