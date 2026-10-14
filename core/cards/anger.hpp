#pragma once
#include "../hooks.hpp"
#include "../actions/add_card_discard.hpp"

namespace spire::units::cards {
struct Anger : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 0, 6, -1, -1, 2, 0, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
        Effect e{};
        e.kind = EffectKind::VFX;
        e.source = e.target = player_target;
        add_bottom(g, e);
        add_bottom(g, actions::AddCardDiscard::make(create_card(g, card_makeStatEquivalentCopy(g, card)), 1));
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += data().upgrade_damage;
        }
    }
};
}
