#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct ReinforcedBody : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, -1, -1, 7, -1, 0, 2, 0, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::ReinforcedBody;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.amount = g.cards[card].block;
        e.payload.state.ReinforcedBody.energyOnUse = g.cards[card].energy_on_use;
        e.payload.state.ReinforcedBody.freeToPlayOnce = g.cards[card].free_to_play_once;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
