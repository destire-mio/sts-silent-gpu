#pragma once
#include "../hooks.hpp"

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
            card.cost = card.cost_for_turn = 3;
            card.upgraded_cost = true;
        }
    }
};
}
