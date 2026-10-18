#pragma once
#include "core/hooks.hpp"
#include "core/actions/draw.hpp"

namespace spire::units::powers {
struct Heatsink : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int owner, int card, Effect &) {
        if (card_data(g.cards[card].id).type == CardType::Power) {
            add_top(g, units::actions::Draw::construct(p.amount, true, -1, owner));
        }
    }
    static SPIRE_FN void stackPower(Game &, Power& p, int, int amount) { p.amount += amount; }
};
}
