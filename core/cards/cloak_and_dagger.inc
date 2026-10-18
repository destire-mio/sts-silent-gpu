#pragma once
#include "core/hooks.hpp"
#include "core/cards/shiv.hpp"

namespace spire::units::cards {
struct CloakAndDagger : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 6, 1, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        Effect shivs{};
        shivs.kind = EffectKind::MakeTempCardInHand;
        shivs.card = create_card(g, Shiv::make(g));
        shivs.amount = g.cards[card].magic;
        add_bottom(g, shivs);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::CloakAndDagger); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
