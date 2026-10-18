#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct FTL : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 0, 5, -1, 3, 1, 0, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        Effect e{};
        e.kind = EffectKind::FTL;
        action_initialize_state(e);
        e.source = player_target;
        e.target = no_target;
        e.extra = target;
        e.amount = e.info_base = g.cards[card].damage;
        e.payload.state.FTL.cardPlayCount = g.cards[card].magic;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_damage += 1;
            card.base_magic += 1;
            card.magic = card.base_magic;
        }
    }
};
}
