#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Draw : ActionHooks {
    SPIRE_STATE(Draw, bool shuffle_check;)
    SPIRE_FN static void prepare(Game&, Effect& e) { e.duration = 0.2f; }
    SPIRE_FN static void execute(Game& g, Effect& e) { draw_cards(g, e); }
};
}
