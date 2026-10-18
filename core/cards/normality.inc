#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Normality : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Curse, CardTarget::None, -2, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game&, int, int) {}
    static SPIRE_FN bool canPlay(const Game& g, int, int) {
        return g.manager.played_cards.size - g.manager.played_cards.turn_start < 3;
    }
    static SPIRE_FN void applyPowers(Game& g, int card) { apply_card_powers(g, card); }
    static SPIRE_FN void upgrade(Card&) {}
};
}
