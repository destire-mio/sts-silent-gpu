#pragma once
#include "../hooks.hpp"

namespace spire::units::cards {
struct JustLucky : CardHooks {
    static SPIRE_FN constexpr CardDef data() {
        return {CardType::Attack, CardTarget::Enemy, 0, 3, 2, 1, 1, 1, 1, false, false};
    }
    static SPIRE_FN void use(Game& g, int card, int target) {
        Effect e{};
        e.kind = EffectKind::Scry;
        e.source = e.target = no_target;
        e.amount = g.cards[card].magic;
        add_bottom(g, e);
        Effect visual{};
        visual.kind = EffectKind::VFX;
        visual.source = visual.target = no_target;
        add_bottom(g, visual);
        queue_block(g, player_target, g.cards[card].block);
        queue_card_damage(g, card, target);
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
