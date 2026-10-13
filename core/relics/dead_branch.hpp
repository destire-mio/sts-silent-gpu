#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct DeadBranch : RelicHooks {
    static SPIRE_FN void onExhaust(Game& g, int relic, int) {
        if (!monsters_basically_dead(g)) {
            queue_relic_above(g, relic, player_target, false);
            Effect e{};
            e.kind = EffectKind::MakeTempCardInHand;
            action_initialize_state(e);
            e.card = create_card(g, make_card(card_id(random_card_in_combat(g, -1, false))));
            e.amount = 1;
            e.state.MakeTempCardInHand.other_card_not_in_center = true;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
};
}
