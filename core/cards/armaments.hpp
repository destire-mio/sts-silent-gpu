#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Armaments : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 1, -1, 5, -1, 0, 0, 0, false, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        Effect e{};
        e.kind = EffectKind::Armaments;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.Armaments.upgraded = g.cards[card].upgrades;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
