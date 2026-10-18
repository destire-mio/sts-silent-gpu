#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Omniscience : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::None, 4, -1, -1, 2, 0, 0, 0, true, false};
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        {
            Effect e{};
            e.kind = EffectKind::Omniscience;
            action_initialize_state(e);
            e.source = e.target = no_target;
            e.amount = 0;
            e.extra = g.cards[card].magic;
            add_bottom(g, e);
        }
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            upgrade_base_cost(card, 3);
        }
    }
};
}
