#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct DecreaseMaxOrb : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    static SPIRE_FN Effect make(int amount) {
        Effect e{}; e.kind = EffectKind::DecreaseMaxOrb; e.source = e.target = no_target; e.amount = amount; return e;
    }
    static SPIRE_FN void execute(Game& g, const Effect& e) {
        for (int i = 0; i < e.amount; ++i) decrease_max_orb_slots(g, 1);
    }
};
}
