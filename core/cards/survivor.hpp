#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct Survivor : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Skill, CardTarget::Self, 1, -1, 8, -1, 0, 3, 0, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int) {
        queue_block(g, player_target, g.cards[card].block);
        Effect discard{};
        discard.kind = EffectKind::Discard;
        discard.amount = 1;
        add_bottom(g, discard);
    }
    static SPIRE_FN void upgrade(Card& card) {
        if (card.upgrades == 0) {
            ++card.upgrades;
            card.base_block += data().upgrade_block;
        }
    }
};
}
