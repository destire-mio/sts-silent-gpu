#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct TransmutationAction : ActionHooks {
    SPIRE_STATE(TransmutationAction, int energy_on_use; int p; bool upgraded; bool free_to_play_once;)
    SPIRE_FN static Effect construct(int player, bool upgraded, bool free, int energy) {
        Effect e{};
        e.kind = EffectKind::TransmutationAction;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.state.TransmutationAction.p = player;
        e.state.TransmutationAction.upgraded = upgraded;
        e.state.TransmutationAction.free_to_play_once = free;
        e.state.TransmutationAction.energy_on_use = energy;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        const auto& state = e.state.TransmutationAction;
        int effect = state.energy_on_use == -1 ? g.player.energy : state.energy_on_use;
        if (has_relic(g, "Chemical X")) effect += 2;
        if (effect > 0 && !state.free_to_play_once) g.player.energy = 0;
        e.done = true;
    }
};
}
