#pragma once
#include "../hooks.hpp"

namespace spire::units::actions {
struct Halt : ActionHooks {
    SPIRE_SNAPSHOT("HaltAction.additionalAmt", scalar, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        {
            Effect a{};
            a.kind = EffectKind::Block;
            a.source = no_target;
            a.target = e.target;
            a.amount = e.amount + (g.player.stance == Stance::Wrath ? e.extra : 0);
            add_top(g, a);
        }
    }
};
}
