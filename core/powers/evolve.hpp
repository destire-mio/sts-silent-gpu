#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::powers {
struct Evolve : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onCardDraw(Game& g, Power& p, int owner, int card) {
        if (card_data(g.cards[card].id).type == CardType::Status &&
            !has_power(g, get_creature(g, owner), PowerId::NoDraw)) {
            add_bottom(g, units::actions::Draw::construct(p.amount, true, -1, owner));
        }
    }
};
}
