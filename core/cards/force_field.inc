#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct ForceField : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 4, -1, 12, -1, 0, 4, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
    static SPIRE_FN void constructCard(const Game& g, Card& card) {
        for (int i = 0; i < g.manager.played_cards.size; ++i)
            if (card_data(g.cards[g.manager.played_cards.values[i]].id).type == CardType::Power)
                reduce_cost(card);
    }
    static SPIRE_FN void reduce_cost(Card& card) {
        const int difference = card.cost - card.cost_for_turn;
        card.cost = max(0, card.cost - 1);
        card.cost_for_turn = max(0, card.cost - difference);
    }
    static SPIRE_FN void triggerOnCardPlayed(Game& g, int card, int other_card) {
        if (card_data(g.cards[other_card].id).type == CardType::Power)
            reduce_cost(g.cards[card]);
    }
};
}
