#pragma once
#include "core/hooks.hpp"
#include "core/actions/draw.hpp"
#include "core/actions/change_stance.hpp"

namespace spire::units::actions {
struct InnerPeace : ActionHooks {
    static SPIRE_FN void execute(Game& g, Effect& e) {
        if (g.player.stance == Stance::Calm)
            add_top(g, units::actions::Draw::construct(e.amount));
        else
            add_top(g, units::actions::ChangeStance::make(Stance::Calm));
    }
};
}
