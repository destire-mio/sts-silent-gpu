#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Shiv : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 0, 4, -1, -1, 2, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN Card make(const Game& g) {
        Card card = make_card(CardId::Shiv);
        card.base_damage += power_amount(g, g.player.creature, PowerId::Accuracy);
        return card;
    }
    static SPIRE_FN Card makeCopy(const Game& g, int) { return make(g); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
