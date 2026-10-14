#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct IncreaseMaxOrb : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    static SPIRE_FN Effect make(int amount) {
        Effect e{}; e.kind = EffectKind::IncreaseMaxOrb; e.source = e.target = no_target; e.amount = amount; return e;
    }
    static SPIRE_FN void execute(Game& g, const Effect& e) {
        for (int i = 0; i < e.amount; ++i) increase_max_orb_slots(g, 1);
    }
};
}
