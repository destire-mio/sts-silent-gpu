#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Distraction : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        Card generated = make_card(card_id(random_card_in_combat(g, static_cast<int>(CardType::Skill), false)));
        if (generated.cost_for_turn >= 0) generated.cost_for_turn = 0;
        Effect e{};
        e.kind = EffectKind::MakeTempCardInHand;
        e.card = create_card(g, generated);
        e.amount = 1;
        action_initialize_state(e);
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.cost = 0;
            card.cost_for_turn = card.cost;
        }
    }
};
}
