#pragma once
#include "../hooks.hpp"
#include "../actions/not_stance_check.hpp"
#include "../actions/change_stance.hpp"

namespace spire::units::cards {
struct EmptyFist : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 1, 9, -1, -1, 5, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect visual{};
        visual.kind = EffectKind::VFX;
        visual.source = visual.target = no_target;
        add_bottom(g, actions::NotStanceCheck::make(g, Stance::Neutral, visual));
        add_bottom(g, actions::ChangeStance::make(Stance::Neutral));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 5;
        }
    }
};
}
