#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Hex : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {5, true}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int, int card, Effect&) {
        if (card_data(g.cards[card].id).type != CardType::Attack) {
            Effect e{};
            e.kind = EffectKind::MakeTempCardInDrawPile;
            action_initialize_state(e);
            e.card = create_card(g, make_card(CardId::Dazed));
            e.amount = p.amount;
            e.state.MakeTempCardInDrawPile.random_spot = true;
            add_bottom(g, e);
        }
    }
};
}
