#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Channel : ActionHooks {
    SPIRE_SNAPSHOT("ChannelAction.orbType", orb, effect.extra)
    SPIRE_SNAPSHOT("ChannelAction.autoEvoke", scalar, effect.flag)
    static SPIRE_FN Effect make(int orb, bool auto_evoke = true) {
        Effect e{}; e.kind = EffectKind::Channel; e.source = e.target = no_target;
        e.extra = orb; e.flag = auto_evoke; return e;
    }
    static SPIRE_FN void execute(Game& g, const Effect& e) {
        if (e.flag || has_empty_orb(g)) channel_orb(g, e.extra);
    }
};
}
