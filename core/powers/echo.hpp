#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Echo : PowerHooks {
    SPIRE_WAITING_RULE("EchoPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    SPIRE_STATE(Echo, int cardsDoubledThisTurn;)
    static SPIRE_FN void atStartOfTurn(Game &, Power& p, int) { p.state.Echo.cardsDoubledThisTurn = 0; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int, int card, Effect& use) {
        if (!g.cards[card].purge_on_use && p.amount > 0 &&
            g.manager.played_cards.size - g.manager.played_cards.turn_start - p.state.Echo.cardsDoubledThisTurn <=
                p.amount) {
            ++p.state.Echo.cardsDoubledThisTurn;
            queue_card_copy(g, card, use.target);
        }
    }
};
}
