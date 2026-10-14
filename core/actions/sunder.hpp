#pragma once
#include "../hooks.hpp"
#include "gain_energy.hpp"

namespace spire::units::actions {
struct Sunder : ActionHooks {
    SPIRE_WAITING_RULE("SunderAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr ActionData data() { return {true, "DAMAGE"}; }
    SPIRE_SNAPSHOT("SunderAction.info", damage_info, effect, queue, index)
    SPIRE_SNAPSHOT("SunderAction.energyGainAmt", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        creature_damage(g, e);
        if (get_creature(g, e.target).dying || get_creature(g, e.target).hp <= 0)
            add_bottom(g, units::actions::GainEnergy::make(e.extra));
        if (monsters_basically_dead(g))
            clear_post_combat_actions(g);
    }
};
}
