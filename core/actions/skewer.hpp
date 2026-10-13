#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct SkewerAction : ActionHooks {
    SPIRE_SNAPSHOT("SkewerAction.p", creature, effect.state.SkewerAction.p)
    SPIRE_SNAPSHOT("SkewerAction.m", creature, effect.state.SkewerAction.m)
    SPIRE_SNAPSHOT("SkewerAction.damageTypeForTurn", damage_type, effect.state.SkewerAction.damage_type_for_turn)
    static SPIRE_FN constexpr ActionData data() { return {false, "SPECIAL"}; }
    SPIRE_STATE(SkewerAction, int damage; DamageType damage_type_for_turn; int energy_on_use; int p; int m; bool free_to_play_once;)
    SPIRE_FN static Effect construct(int player, int monster, int damage, DamageType type, bool free, int energy) {
        Effect e{};
        e.kind = EffectKind::SkewerAction;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.state.SkewerAction.p = player;
        e.state.SkewerAction.m = monster;
        e.state.SkewerAction.damage = damage;
        e.state.SkewerAction.damage_type_for_turn = type;
        e.state.SkewerAction.free_to_play_once = free;
        e.state.SkewerAction.energy_on_use = energy;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        const auto& state = e.state.SkewerAction;
        int effect = state.energy_on_use == -1 ? g.player.energy : state.energy_on_use;
        if (has_relic(g, "Chemical X")) effect += 2;
        if (effect > 0) {
            for (int i = 0; i < effect; ++i) {
                Effect hit{};
                hit.kind = EffectKind::Damage;
                hit.source = state.p;
                hit.target = state.m;
                hit.amount = hit.info_base = state.damage;
                hit.damage_type = state.damage_type_for_turn;
                add_bottom(g, hit);
            }
            if (!state.free_to_play_once) g.player.energy = 0;
        }
        e.done = true;
    }
};
}
