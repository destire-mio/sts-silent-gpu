#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ExhaustCard : ActionHooks {
    SPIRE_FN static void execute(Game& g, const Effect& e) { exhaust_card(g, e.card); }
};
}
