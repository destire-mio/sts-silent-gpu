#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct MalaiseAction : ActionHooks {
    SPIRE_STATE(MalaiseAction, int energy_on_use; int p; int m; bool upgraded; bool free_to_play_once;)
    SPIRE_FN static Effect construct(int player, int monster, bool upgraded, bool free, int energy) {
        Effect e{};
        e.kind = EffectKind::MalaiseAction;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.state.MalaiseAction.p = player;
        e.state.MalaiseAction.m = monster;
        e.state.MalaiseAction.upgraded = upgraded;
        e.state.MalaiseAction.free_to_play_once = free;
        e.state.MalaiseAction.energy_on_use = energy;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        const auto& state = e.state.MalaiseAction;
        int effect = state.energy_on_use == -1 ? g.player.energy : state.energy_on_use;
        if (has_relic(g, "Chemical X")) effect += 2;
        if (state.upgraded) ++effect;
        if (effect > 0) {
            Effect strength{};
            strength.kind = EffectKind::ApplyPower;
            strength.source = state.p;
            strength.target = state.m;
            strength.power = PowerId::Strength;
            strength.amount = -effect;
            add_bottom(g, strength);
            Effect weak{};
            weak.kind = EffectKind::ApplyPower;
            weak.source = state.p;
            weak.target = state.m;
            weak.power = PowerId::Weak;
            weak.amount = effect;
            add_bottom(g, weak);
            if (!state.free_to_play_once) g.player.energy = 0;
        }
        e.done = true;
    }
};
}
