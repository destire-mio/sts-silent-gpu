#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct Mark : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN void triggerMarks(Game& g, Power& p, int owner, int card) {
        if (g.cards[card].id == CardId::PressurePoints) {
            Effect e{};
            e.kind = EffectKind::LoseHP;
            e.source = no_target;
            e.target = owner;
            e.amount = p.amount;
            add_bottom(g, e);
        }
    }
};
}
