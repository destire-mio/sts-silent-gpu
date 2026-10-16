#pragma once
#include "../hooks.hpp"
#include "gain_energy.hpp"

namespace spire::units::actions {
struct HeelHookAction : ActionHooks {
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("HeelHookAction.info", damage_info, effect, queue, index)
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    SPIRE_FN static Effect construct(int target, const DamageInfo& info) {
        Effect e{};
        e.kind = EffectKind::HeelHookAction;
        action_initialize_state(e);
        e.target = target;
        e.source = info.source;
        e.amount = info.amount;
        e.info_base = info.info_base;
        e.damage_type = info.damage_type;
        return e;
    }
    SPIRE_FN static void execute(Game& g, Effect& e) {
        if (e.target != no_target && find_power(g, get_creature(g, e.target), PowerId::Weak) >= 0) {
            Effect draw{};
            draw.kind = EffectKind::Draw;
            draw.source = player_target;
            draw.amount = 1;
            add_top(g, draw);
            add_top(g, GainEnergy::make(1));
        }
        Effect hit{};
        hit.kind = EffectKind::Damage;
        hit.source = e.source;
        hit.target = e.target;
        hit.amount = e.amount;
        hit.info_base = e.info_base;
        hit.damage_type = e.damage_type;
        add_top(g, hit);
        e.done = true;
    }
};
}
