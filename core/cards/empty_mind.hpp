#pragma once
#include "../hooks.hpp"
#include "../actions/not_stance_check.hpp"
#include "../actions/change_stance.hpp"
#include "../actions/draw.hpp"

namespace spire::units::cards {
struct EmptyMind : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, -1, 2, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        add_bottom(g, actions::Draw::construct(g.cards[card].magic));
        Effect visual{};
        visual.kind = EffectKind::VFX;
        visual.source = visual.target = no_target;
        add_bottom(g, actions::NotStanceCheck::make(g, Stance::Neutral, visual));
        add_bottom(g, actions::ChangeStance::make(Stance::Neutral));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
