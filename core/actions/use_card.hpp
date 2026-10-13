#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct UseCard : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true}; }
    SPIRE_FN static void execute(Game& g, const Effect& e) { finish_card_use(g, e); }
};
}
