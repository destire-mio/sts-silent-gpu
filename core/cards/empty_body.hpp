#pragma once
#include "../hooks.hpp"
#include "../actions/not_stance_check.hpp"
#include "../actions/change_stance.hpp"

namespace spire::units::cards {
struct EmptyBody : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 7, -1, 0, 3, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        Effect visual{};
        visual.kind = EffectKind::VFX;
        visual.source = visual.target = no_target;
        add_bottom(g, actions::NotStanceCheck::make(g, Stance::Neutral, visual));
        add_bottom(g, actions::ChangeStance::make(Stance::Neutral));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
