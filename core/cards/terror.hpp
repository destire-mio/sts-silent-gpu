#pragma once
#include "../hooks.hpp"

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
            const int diff = card.cost_for_turn - card.cost;
            card.cost = 0;
            if (card.cost_for_turn > 0) card.cost_for_turn = diff;
            if (card.cost_for_turn < 0) card.cost_for_turn = 0;
        }
    }
};
}
