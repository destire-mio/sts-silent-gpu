#pragma once
#include "../hooks.hpp"

namespace spire::units::powers {
struct Accuracy : PowerHooks {
    static SPIRE_FN constexpr PowerData data() { return {}; }
    static SPIRE_FN void updateShivs(Game& g, const Power& p, const Pile& pile) {
        for (int i = 0; i < pile.size; ++i) {
            Card& card = g.cards[pile[i]];
            if (card.id == CardId::Shiv) card.base_damage = (card.upgrades == 0 ? 4 : 6) + p.amount;
        }
    }
    static SPIRE_FN void updateExistingShivs(Game& g, const Power& p) {
        updateShivs(g, p, g.hand);
        updateShivs(g, p, g.draw);
        updateShivs(g, p, g.discard);
        updateShivs(g, p, g.exhaust);
    }
    static SPIRE_FN void constructPower(Game& g, Power& p, int, int, int, int) {
        power_initialize_state(p);
        updateExistingShivs(g, p);
    }
    static SPIRE_FN void stackPower(Game& g, Power& p, int, int amount) {
        p.amount += amount;
        updateExistingShivs(g, p);
    }
    static SPIRE_FN void onDrawOrDiscard(Game& g, Power& p, int) { updateShivs(g, p, g.hand); }
};
}
