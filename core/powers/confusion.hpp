#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Confusion : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {0, true}; }
    static SPIRE_FN void initialPower(Game&, Power& p, int) { p.amount = -1; }
    static SPIRE_FN void onCardDraw(Game& g, Power&, int, int card) {
        Card& c = g.cards[card];
        if (c.cost >= 0) {
            const int cost = g.rng.card_random.random(3);
            if (c.cost != cost) c.cost_for_turn = c.cost = cost;
            c.free_to_play_once = false;
        }
    }
};
}
