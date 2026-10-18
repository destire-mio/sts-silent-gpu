#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Accuracy : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 1, -1, -1, 4, 0, 0, 2, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Accuracy, g.cards[card].magic);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::Accuracy); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
