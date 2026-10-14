#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Corruption : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void initialPower(Game &, Power& p, int) { p.amount = -1; }
    static SPIRE_FN void onCardDraw(Game& g, Power &, int, int card) {
        if (card_data(g.cards[card].id).type == CardType::Skill && g.cards[card].cost_for_turn >= 0)
            g.cards[card].cost_for_turn = 0;
    }
    static SPIRE_FN void onUseCard(Game& g, Power &, int, int card, Effect& use) {
        if (card_data(g.cards[card].id).type == CardType::Skill)
            use.flag = true;
    }
};
}
