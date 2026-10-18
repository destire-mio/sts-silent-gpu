#pragma once
#include "core/hooks.hpp"
#include "core/actions/exhaust_card.hpp"

namespace spire::units::cards {
struct Clumsy : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Curse, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, true};
    }
    static SPIRE_FN void use(Game&, int, int) {}
    static SPIRE_FN void upgrade(Card&) {}
    static SPIRE_FN void triggerOnEndOfPlayerTurn(Game& g, int card) {
        add_top(g, actions::ExhaustCard::make(card, PileId::Hand));
    }
};
}
