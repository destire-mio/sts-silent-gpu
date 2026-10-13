#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Expertise : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    SPIRE_FN static Effect construct(int source, int amount) {
        Effect e{};
        e.kind = EffectKind::Expertise;
        action_initialize_state(e);
        e.target = no_target;
        e.source = source;
        e.amount = amount;
        return e;
    }
    SPIRE_FN static void prepare(Game&, Effect& e) { e.target = no_target; }
    SPIRE_FN static void execute(Game& g, const Effect& e) {
        const int amount = e.amount - g.hand.size;
        if (amount > 0) add_top(g, Draw::construct(amount, true, -1, e.source));
    }
};
}
