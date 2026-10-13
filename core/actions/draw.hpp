#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Draw : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) { draw_cards(g, e); }
};
}
