#pragma once
#include "../hooks.hpp"
#include "draw.hpp"

namespace spire::units::actions {
struct FTL : ActionHooks {
    SPIRE_WAITING_RULE("FTLAction: waiting for a ported caller with three-seed oracle replay")
    SPIRE_STATE(FTL, int cardPlayCount;)
    SPIRE_SNAPSHOT("FTLAction.info", damage_info, effect, queue, index)
    SPIRE_SNAPSHOT("FTLAction.target", creature, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        Effect hit = e;
        hit.kind = EffectKind::Damage;
        action_initialize_state(hit);
        hit.target = e.extra;
        add_bottom(g, hit);
        if (g.manager.played_cards.size - g.manager.played_cards.turn_start - 1 < e.payload.state.FTL.cardPlayCount)
            add_top(g, units::actions::Draw::construct(1));
    }
};
}
