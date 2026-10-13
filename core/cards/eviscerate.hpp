#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Eviscerate : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 3, 7, -1, -1, 2, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        queue_card_damage(g, card, target);
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void didDiscard(Game& g, int card) {
        Card& c = get_card(g, card);
        if (c.cost_for_turn >= 0) c.cost_for_turn = max(0, c.cost_for_turn - 1);
    }
    static SPIRE_FN void triggerWhenDrawn(Game& g, int card) {
        Card& c = get_card(g, card);
        if (c.cost_for_turn >= 0) c.cost_for_turn = max(0, c.cost - g.manager.total_discarded_this_turn);
    }
    static SPIRE_FN void atTurnStart(Game& g, int card) {
        card_resetAttributes(g, card);
        card_applyPowers(g, card);
    }
    static SPIRE_FN Card makeCopy(Game& g, int card) {
        Card copy = make_card(CardId::Eviscerate);
        // Eviscerate.makeCopy changes the source, not the returned card.
        if (!g.battle_over && g.monster_order.size > 0) triggerWhenDrawn(g, card);
        return copy;
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
