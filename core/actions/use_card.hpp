#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct UseCard : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {true}; }
    SPIRE_FN static void prepare(Game&, Effect& e) {
        e.amount = 1;
        e.source = no_target;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        finish_card_use(g, e);
    }
};
}
