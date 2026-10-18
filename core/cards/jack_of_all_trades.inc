#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct JackOfAllTrades : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 0, -1, -1, 1, 0, 0, 1, true, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        for (int i = 0; i < (g.cards[card].upgrades > 0 ? 2 : 1); ++i) {
            Effect e{};
            e.kind = EffectKind::MakeTempCardInHand;
            e.card = create_card(g, make_card(random_card_in_combat(g, -1, true)));
            e.amount = 1;
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
