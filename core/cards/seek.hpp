#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Seek : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::None, 0, -1, -1, 1, 0, 0, 1, true, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::BetterDrawPileToHand;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.payload.state.BetterDrawPileToHand.numberOfCards = g.cards[card].magic;
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
