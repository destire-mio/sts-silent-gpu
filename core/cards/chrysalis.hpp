#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Chrysalis : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 2, -1, -1, 3, 0, 0, 2, true, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        for (int i = 0; i < g.cards[card].magic; ++i) {
            Card created = make_card(card_id(random_card_in_combat(g, static_cast<int>(CardType::Skill), false)));
            if (created.cost > 0) created.cost = created.cost_for_turn = 0;
            Effect e{};
            e.kind = EffectKind::MakeTempCardInDrawPile;
            action_initialize_state(e);
            e.card = create_card(g, created);
            e.amount = 1;
            e.payload.state.MakeTempCardInDrawPile.random_spot = true;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
