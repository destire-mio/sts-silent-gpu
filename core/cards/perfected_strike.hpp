#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct PerfectedStrike : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 2, 6, -1, 2, 0, 0, 1, false, false};
        d.strike = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
    static SPIRE_FN int countCards(const Game& g) {
        int count = 0;
        const PileId piles[] = {PileId::Hand, PileId::Draw, PileId::Discard};
        for (int p = 0; p < 3; ++p) {
            const Pile& pile = get_pile(g, piles[p]);
            for (int i = 0; i < pile.size; ++i)
                if (card_def(g.cards[pile[i]].id).strike) ++count;
        }
        return count;
    }
    static SPIRE_FN void applyPowers(Game& g, int card) {
        const int base = g.cards[card].base_damage;
        g.cards[card].base_damage += g.cards[card].magic * countCards(g);
        CardHooks::applyPowers(g, card);
        g.cards[card].base_damage = base;
    }
    static SPIRE_FN void calculateCardDamage(Game& g, int card, int target) {
        const int base = g.cards[card].base_damage;
        g.cards[card].base_damage += g.cards[card].magic * countCards(g);
        CardHooks::calculateCardDamage(g, card, target);
        g.cards[card].base_damage = base;
    }
};
}
