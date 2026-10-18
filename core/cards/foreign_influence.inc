#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct ForeignInfluence : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 0, -1, -1, -1, 0, 0, 0, true, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::ForeignInfluence;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.ForeignInfluence.upgraded = g.cards[card].upgrades != 0;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
