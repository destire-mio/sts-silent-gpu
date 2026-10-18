#pragma once
#include "core/hooks.hpp"
#include "core/actions/change_stance.hpp"

namespace spire::units::cards {
struct Meditate : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 1, -1, -1, 1, 0, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::Meditate;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.extra = g.cards[card].magic;
        add_bottom(g, e);
        add_bottom(g, actions::ChangeStance::make(Stance::Calm));
        e = {};
        e.kind = EffectKind::PressEndTurnButton;
        e.source = e.target = no_target;
        add_bottom(g, e);
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
