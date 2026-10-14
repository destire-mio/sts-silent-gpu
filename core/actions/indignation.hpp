#pragma once
#include "../hooks.hpp"
#include "change_stance.hpp"

namespace spire::units::actions {
struct Indignation : ActionHooks {
    SPIRE_WAITING_RULE("IndignationAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (g.player.stance == Stance::Wrath) {
            for (int i = 0; i < g.monster_order.size; ++i)
                queue_power(g, player_target, g.monster_order[i], PowerId::Vulnerable, e.amount);
        } else
            add_bottom(g, units::actions::ChangeStance::make(Stance::Wrath));
    }
};
}
