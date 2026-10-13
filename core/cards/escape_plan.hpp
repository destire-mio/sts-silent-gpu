#pragma once
#include "../hooks.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct EscapePlan : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, 3, -1, 0, 2, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect follow_up{};
        follow_up.kind = EffectKind::EscapePlanAction;
        follow_up.amount = g.cards[card].block;
        add_bottom(g, actions::Draw::construct(g, 1, follow_up));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
