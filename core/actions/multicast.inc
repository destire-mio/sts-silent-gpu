#pragma once
#include "core/hooks.hpp"

namespace spire::units::actions {
struct Multicast : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(Multicast, int energyOnUse; bool freeToPlayOnce; bool upgraded;)
    SPIRE_SNAPSHOT("MulticastAction.p", player)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (!has_orb(g))
            return;
        int count = e.payload.state.Multicast.energyOnUse < 0 ? g.player.energy : e.payload.state.Multicast.energyOnUse;
        if (has_relic(g, RelicId::ChemicalX))
            count += 2;
        if (e.payload.state.Multicast.upgraded)
            ++count;
        if (count > 0) {
            for (int i = 0; i < count - 1; ++i) {
                Effect a{};
                a.kind = EffectKind::EvokeWithoutRemovingOrb;
                a.extra = 1;
                a.source = a.target = no_target;
                add_bottom(g, a);
            }
            Effect a{};
            a.kind = EffectKind::AnimateOrb;
            a.extra = 1;
            a.source = a.target = no_target;
            add_bottom(g, a);
            a.kind = EffectKind::EvokeOrb;
            add_bottom(g, a);
            if (!e.payload.state.Multicast.freeToPlayOnce)
                g.player.energy = 0;
        }
    }
};
}
