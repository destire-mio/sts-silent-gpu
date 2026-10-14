#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::powers {
struct Heatsink : PowerHooks {
    SPIRE_WAITING_RULE("HeatsinkPower: waiting for a ported caller with three-seed oracle replay")
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void onUseCard(Game& g, Power& p, int owner, int card, Effect &) {
        if (card_data(g.cards[card].id).type == CardType::Power) {
            add_top(g, units::actions::Draw::construct(p.amount, true, -1, owner));
        }
    }
    static SPIRE_FN void stackPower(Game &, Power& p, int, int amount) { p.amount += amount; }
};
}
