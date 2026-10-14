#pragma once
#include "../hooks.hpp"
#include "draw.hpp"
#include "gain_energy.hpp"

namespace spire::units::actions {
struct Dropkick : ActionHooks {
    static SPIRE_FN constexpr ActionData data() { return {false, "BLOCK"}; }
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("DropkickAction.info", damage_info, effect, queue, index)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (e.target != no_target && has_power(g, get_creature(g, e.target), PowerId::Vulnerable)) {
            add_top(g, units::actions::Draw::construct(1));
            add_top(g, units::actions::GainEnergy::make(1));
        }
        Effect hit = e;
        hit.kind = EffectKind::Damage;
        action_initialize_state(hit);
        add_top(g, hit);
    }
};
}
