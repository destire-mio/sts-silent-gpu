#pragma once
#include "core/hooks.hpp"

namespace spire::units::powers {
struct CreativeAI : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void atStartOfTurn(Game& g, Power& p, int) {
        for (int i = 0; i < p.amount; ++i) {
            const CardId id = random_card_in_combat(g, static_cast<int>(CardType::Power), false);
            Effect e{};
            e.kind = EffectKind::MakeTempCardInHand;
            action_initialize_state(e);
            e.card = create_card(g, make_card(id));
            e.amount = 1;
            add_bottom(g, e);
        }
    }
};
}
