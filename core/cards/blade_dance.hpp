#pragma once
#include "../hooks.hpp"
#include "shiv.hpp"

namespace spire::units::cards {
struct BladeDance : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, 3, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect shivs{};
        shivs.kind = EffectKind::MakeTempCardInHand;
        shivs.card = create_card(g, Shiv::make(g));
        shivs.amount = g.cards[card].magic;
        add_bottom(g, shivs);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::BladeDance); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
