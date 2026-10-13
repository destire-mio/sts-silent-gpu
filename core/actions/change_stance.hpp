#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct ChangeStance : ActionHooks {
    SPIRE_SNAPSHOT("ChangeStanceAction.id", stance_name, static_cast<Stance>(effect.extra))
    SPIRE_SNAPSHOT("ChangeStanceAction.newStance", scalar, nullptr)
    static SPIRE_FN Effect make(Stance stance) {
        Effect e{}; e.kind = EffectKind::ChangeStance; e.source = e.target = no_target;
        e.extra = static_cast<int>(stance); return e;
    }
    static SPIRE_FN void execute(Game& g, const Effect& e) { change_stance(g, static_cast<Stance>(e.extra)); }

};
}
