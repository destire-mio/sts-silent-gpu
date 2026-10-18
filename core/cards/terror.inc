#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Terror : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Enemy, 1, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int, int target) {
        queue_power(g, player_target, target, PowerId::Vulnerable, 99);
    }
    static SPIRE_FN Card makeCopy(const Game&, int) { return make_card(CardId::Terror); }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 0);
        }
    }
};
}
