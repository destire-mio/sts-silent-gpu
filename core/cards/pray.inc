#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Pray : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 1, -1, -1, 3, 0, 0, 1, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Mantra, g.cards[card].magic);
        Effect e{};
        e.kind = EffectKind::MakeTempCardInDrawPile;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = create_card(g, make_card(CardId::Insight, 0));
        e.amount = 1;
        e.payload.state.MakeTempCardInDrawPile.random_spot = true;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
