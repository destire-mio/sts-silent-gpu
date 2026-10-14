#pragma once
#include "../hooks.hpp"
#include "channel.hpp"

namespace spire::units::actions {
struct Tempest : ActionHooks {
    SPIRE_WAITING_RULE("TempestAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(Tempest, int energyOnUse; bool freeToPlayOnce; bool upgraded;)
    SPIRE_SNAPSHOT("TempestAction.p", player)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        int count = e.payload.state.Tempest.energyOnUse < 0 ? g.player.energy : e.payload.state.Tempest.energyOnUse;
        if (has_relic(g, "Chemical X"))
            count += 2;
        if (e.payload.state.Tempest.upgraded)
            ++count;
        if (count > 0) {
            for (int i = 0; i < count; ++i)
                add_bottom(g, units::actions::Channel::make(make_orb(g, OrbKind::Lightning)));
            if (!e.payload.state.Tempest.freeToPlayOnce)
                g.player.energy = 0;
        }
    }
};
}
