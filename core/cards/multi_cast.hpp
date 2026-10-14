#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct MultiCast : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, -1, -1, -1, -1, 0, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::Multicast;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.Multicast.energyOnUse = g.cards[card].energy_on_use;
        e.payload.state.Multicast.freeToPlayOnce = g.cards[card].free_to_play_once;
        e.payload.state.Multicast.upgraded = g.cards[card].upgrades != 0;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
