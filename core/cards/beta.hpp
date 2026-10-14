#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Beta : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::None, 2, -1, -1, -1, 0, 0, 0, true, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int, int) {
        Effect e{};
        e.kind = EffectKind::MakeTempCardInDrawPile;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = create_card(g, make_card(CardId::Omega, 0));
        e.amount = 1;
        e.payload.state.MakeTempCardInDrawPile.random_spot = true;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            const int difference = card.cost_for_turn - card.cost;
            card.cost = 1;
            if (card.cost_for_turn > 0) card.cost_for_turn = card.cost + difference;
            if (card.cost_for_turn < 0) card.cost_for_turn = 0;
            card.upgraded_cost = true;
        }
    }
};
}
