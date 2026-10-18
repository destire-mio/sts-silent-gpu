#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct PowerThrough : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 1, -1, 15, -1, 0, 5, 0, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::MakeTempCardInHand;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = create_card(g, make_card(CardId::Wound, 0));
        e.amount = 2;
        add_bottom(g, e);
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
