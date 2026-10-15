#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Wait : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "WAIT"}; }
    static SPIRE_FN Effect make(int frames) {
        Effect e{};
        e.kind = EffectKind::Wait;
        e.amount = frames;
        return e;
    }
    static SPIRE_FN void execute(Game& g, const Effect& e) {
        advance_escape(g, e.amount - 1);
        settle_deaths_after_wait(g, e.amount);
    }
};
}
