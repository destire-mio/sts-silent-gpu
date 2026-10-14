#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Mark : PowerHooks {
    SPIRE_WAITING_RULE("MarkPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN void triggerMarks(Game& g, Power& p, int owner, int card) {
        if (card_is(g.cards[card].id, "PathToVictory")) {
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
