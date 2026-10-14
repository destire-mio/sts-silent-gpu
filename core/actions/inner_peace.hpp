#pragma once
#include "../hooks.hpp"
#include "draw.hpp"
#include "change_stance.hpp"

namespace spire::units::actions {
struct InnerPeace : ActionHooks {
    SPIRE_WAITING_RULE("InnerPeaceAction: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (g.player.stance == Stance::Calm)
            add_top(g, units::actions::Draw::construct(e.amount));
        else
            add_top(g, units::actions::ChangeStance::make(Stance::Calm));
    }
};
}
