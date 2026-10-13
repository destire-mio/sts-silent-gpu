#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Apotheosis : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 2, -1, -1, -1, 0, 0, 0, true, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        Effect e{};
        e.kind = EffectKind::Apotheosis;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            const int difference = card.cost_for_turn - card.cost;
            card.cost = 1;
            if (card.cost_for_turn > 0) card.cost_for_turn = card.cost + difference;
            if (card.cost_for_turn < 0) card.cost_for_turn = 0;
        }
    }
};
}
