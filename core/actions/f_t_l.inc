#pragma once
#include "core/hooks.hpp"
#include "core/actions/draw.hpp"

namespace spire::units::actions {
struct FTL : ActionHooks {
    SPIRE_STATE(FTL, int cardPlayCount;)
    SPIRE_SNAPSHOT("FTLAction.info", damage_info, effect, queue, index)
    SPIRE_SNAPSHOT("AbstractGameAction.amount", scalar, 0)
    SPIRE_SNAPSHOT("AbstractGameAction.source", scalar, nullptr)
    SPIRE_SNAPSHOT("AbstractGameAction.target", scalar, nullptr)
    SPIRE_SNAPSHOT("FTLAction.target", creature, effect.extra)
    static SPIRE_FN void execute(Game& g, Effect& e) {
        Effect hit = e;
        hit.kind = EffectKind::Damage;
        action_initialize_state(hit);
        hit.target = e.extra;
        // FTLAction.update constructs DamageAction without a gold amount.
        hit.extra = 0;
        add_bottom(g, hit);
        if (g.manager.played_cards.size - g.manager.played_cards.turn_start - 1 < e.payload.state.FTL.cardPlayCount)
            add_top(g, units::actions::Draw::construct(1));
    }
};
}
