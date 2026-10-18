#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Entrench : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 2, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int, int) {
        Effect e{};
        e.kind = EffectKind::DoubleYourBlock;
        e.source = no_target;
        e.target = player_target;
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
    static SPIRE_FN void triggerOnEndOfPlayerTurn(Game& g, int) {
        // Entrench.triggerOnEndOfPlayerTurn also queues ethereal exhaustion.
        Effect e{};
        e.kind = EffectKind::ExhaustAllEthereal;
        e.source = e.target = no_target;
        add_top(g, e);
    }
};
}
