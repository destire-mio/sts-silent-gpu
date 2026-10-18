#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct WhiteNoise : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, -1, 0, 0, 0, true, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        Card generated = make_card(random_card_in_combat(g, static_cast<int>(CardType::Power), false));
        if (generated.cost_for_turn >= 0) generated.cost_for_turn = 0;
        Effect e{};
        e.kind = EffectKind::MakeTempCardInHand;
        action_initialize_state(e);
        e.card = create_card(g, generated);
        e.amount = 1;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 0);
        }
    }
};
}
