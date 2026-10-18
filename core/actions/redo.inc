#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct Redo : ActionHooks {
    SPIRE_SNAPSHOT("RedoAction.orb", orb, effect.extra)
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    static SPIRE_FN void prepare(Game&, Effect& e) { e.source = e.target = no_target; e.extra = -1; }
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (has_orb(g)) {
            e.extra = g.orbs.slots[0];
            Effect channel{}; channel.kind = EffectKind::Channel; channel.source = channel.target = no_target; channel.extra = e.extra;
            add_top(g, channel);
            Effect evoke{}; evoke.kind = EffectKind::EvokeOrb; evoke.source = evoke.target = no_target; evoke.extra = 1; add_top(g, evoke);
        }
    }
};
}
