#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct DarkImpulse : ActionHooks {
    static SPIRE_FN void prepare(Game&, Effect& e) { e.source = e.target = no_target; }
    static SPIRE_FN void execute(Game& g, const Effect&) {
        for (int i = 0; i < g.orbs.slots.size; ++i) {
            const int orb = g.orbs.slots[i];
            if (orb >= 0 && g.orbs.objects[orb].kind == OrbKind::Dark) { orb_start_turn(g, orb); orb_end_turn(g, orb); }
        }
        if (has_relic(g, "Cables") && has_orb(g) && g.orbs.objects[g.orbs.slots[0]].kind == OrbKind::Dark) {
            orb_start_turn(g, g.orbs.slots[0]); orb_end_turn(g, g.orbs.slots[0]);
        }
    }
};
}
