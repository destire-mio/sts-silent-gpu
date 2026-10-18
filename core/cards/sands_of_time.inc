#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct SandsOfTime : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Attack, CardTarget::Enemy, 4, 20, -1, -1, 6, 0, 0, false, false};
        d.self_retain = true;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        queue_card_damage(g, card, target);
    }
    static SPIRE_FN void onRetained(Game& g, int card) {
        Effect e{};
        e.kind = EffectKind::ReduceCost;
        action_initialize_state(e);
        e.source = e.target = no_target;
        e.card = card;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 6;
        }
    }
};
}
