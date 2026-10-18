#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct DarkImpulse : ActionHooks {
    static SPIRE_FN void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    static SPIRE_FN void execute(Game& g, const Effect&) {
        for (int i = 0; i < g.orbs.slots.size; ++i) {
            const int orb = g.orbs.slots[i];
            if (orb >= 0 && g.orbs.kind(orb) == OrbKind::Dark) { orb_start_turn(g, orb); orb_end_turn(g, orb); }
        }
    }
};
}
