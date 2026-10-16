#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Collect : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(Collect, int energyOnUse; bool freeToPlayOnce; bool upgraded;)
    SPIRE_SNAPSHOT("CollectAction.p", player)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        int count = e.payload.state.Collect.energyOnUse == -1 ? g.player.energy : e.payload.state.Collect.energyOnUse;
        if (has_relic(g, RelicId::ChemicalX))
            count += 2;
        if (e.payload.state.Collect.upgraded)
            ++count;
        if (count > 0) {
            queue_power(g, player_target, player_target, PowerId::Collect, count);
            if (!e.payload.state.Collect.freeToPlayOnce)
                g.player.energy = 0;
        }
    }
};
}
