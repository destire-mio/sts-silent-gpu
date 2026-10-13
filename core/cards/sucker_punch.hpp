#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct SuckerPunch : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 7, -1, 1, 2, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        queue_power(g, player_target, target, PowerId::Weak, g.cards[card].magic);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::SuckerPunch); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
