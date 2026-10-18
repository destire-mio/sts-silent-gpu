#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Prostrate : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        CardDef d{CardType::Skill, CardTarget::Self, 0, -1, 4, 2, 0, 0, 1, false, false};
        d.initial_block = 4;
        return d;
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_power(g, player_target, player_target, PowerId::Mantra, g.cards[card].magic, false, 0);
        Effect e{};
        e.kind = EffectKind::Block;
        e.source = no_target;
        e.amount = g.cards[card].block;
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
