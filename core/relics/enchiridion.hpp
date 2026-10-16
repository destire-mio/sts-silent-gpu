#pragma once
#include "../hooks.hpp"

namespace spire::units::relics {
struct Enchiridion : RelicHooks {
    static SPIRE_FN void atPreBattle(Game& g, int) {
        Card card = make_card(random_card_in_combat(g, static_cast<int>(CardType::Power), false));
        if (card.cost != -1) card.cost_for_turn = 0;
        Effect e{};
        e.kind = EffectKind::MakeTempCardInHand;
        e.card = create_card(g, card);
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN constexpr RelicData data() { return {-1, 400}; }
};
}
