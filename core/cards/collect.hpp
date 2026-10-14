#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Collect : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, -1, -1, -1, -1, 0, 0, 0, true, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::Collect;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.Collect.energyOnUse = g.cards[card].energy_on_use;
        e.payload.state.Collect.freeToPlayOnce = g.cards[card].free_to_play_once;
        e.payload.state.Collect.upgraded = g.cards[card].upgrades > 0;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
