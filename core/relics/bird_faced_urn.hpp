#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct BirdFacedUrn : RelicHooks {
    static SPIRE_FN constexpr RelicData data() { return {-1, 300}; }
    static SPIRE_FN void onUseCard(Game& g, int relic, int card, Effect&) {
        if (card_def(g.cards[card].id).type == CardType::Power) {
            Effect e{};
            e.kind = EffectKind::Heal;
            e.amount = 2;
            add_top(g, e);
            queue_relic_display(g, relic, player_target, true);
        }
    }
};
}
