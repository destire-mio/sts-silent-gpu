#pragma once
#include "core/hooks.hpp"

namespace spire::units::cards {
struct Sanctity : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 6, 2, 0, 3, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        Effect block{};
        block.kind = EffectKind::Block;
        block.source = no_target;
        block.amount = g.cards[card].block;
        add_bottom(g, block);
        Effect e{};
        e.kind = EffectKind::Sanctity;
        e.source = e.target = no_target;
        e.extra = g.cards[card].magic;
        add_bottom(g, e);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
        }
    }
};
}
