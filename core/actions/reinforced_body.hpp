#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ReinforcedBody : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(ReinforcedBody, int energyOnUse; bool freeToPlayOnce;)
    SPIRE_SNAPSHOT("ReinforcedBodyAction.multiDamage", scalar, nullptr)
    SPIRE_SNAPSHOT("ReinforcedBodyAction.p", player)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        int count = e.payload.state.ReinforcedBody.energyOnUse < 0 ? g.player.energy
                                                                   : e.payload.state.ReinforcedBody.energyOnUse;
        if (has_relic(g, RelicId::ChemicalX))
            count += 2;
        if (count > 0) {
            for (int i = 0; i < count; ++i) {
                Effect a{};
                a.kind = EffectKind::Block;
                a.source = a.target = player_target;
                a.amount = e.amount;
                add_bottom(g, a);
            }
            if (!e.payload.state.ReinforcedBody.freeToPlayOnce)
                g.player.energy = 0;
        }
    }
};
}
