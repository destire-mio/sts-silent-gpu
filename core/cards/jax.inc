#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct JAX : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 0, -1, -1, 2, 0, 0, 1, false, false, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect e{};
        e.kind = EffectKind::LoseHP;
        e.amount = 3;
        add_bottom(g, e);
        queue_power(g, player_target, player_target, PowerId::Strength, g.cards[card].magic);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_magic += data().upgrade_magic;
            card.magic = card.base_magic;
        }
    }
};
}
