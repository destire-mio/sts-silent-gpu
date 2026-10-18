#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct InfiniteBlades : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Power, CardTarget::Self, 1, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        queue_power(g, player_target, player_target, PowerId::InfiniteBlades, 1);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::InfiniteBlades); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.innate = true;
        }
    }
};
}
