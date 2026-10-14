#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct ConjureBlade : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, -1, -1, -1, -1, 0, 0, 0, true, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::ConjureBlade;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.ConjureBlade.energyOnUse = g.cards[card].energy_on_use + g.cards[card].upgrades;
        e.payload.state.ConjureBlade.freeToPlayOnce = g.cards[card].free_to_play_once;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
