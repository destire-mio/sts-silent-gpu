#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct NinjaScroll : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 250}; }
    static SPIRE_FN void atBattleStartPreDraw(Game& g, int relic) {
        queue_relic_above(g, relic, player_target);
        Effect e{};
        e.kind = EffectKind::MakeTempCardInHand;
        action_initialize_state(e);
        e.card = create_card(g, make_card(card_id("Shiv")));
        e.amount = 3;
        e.state.MakeTempCardInHand.other_card_not_in_center = true;
        add_bottom(g, e);
    }
};
}
